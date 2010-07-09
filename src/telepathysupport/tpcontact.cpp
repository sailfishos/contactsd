/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (people-users@projects.maemo.org)
**
** This file is part of contactsd.
**
** If you have questions regarding the use of this file, please contact
** Nokia at people-users@projects.maemo.org.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "tpcontact.h"
#include <QtCore>
#include <QImage>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/PendingContacts>
#include <contactphotocopy.h>

/*!
 * utility method just for usage here
 */
const QString FilePath(QString avatarToken, QString extension)
{
    return ContactPhotoCopy::avatarDir()+"/telepathy_cache"+avatarToken+'.'+extension;
}

class TpContact::TpContactPrivate
{
    public:
    TpContactPrivate() :
            mHasCaps(false),
            mReady(false) {}
    ~TpContactPrivate() {}

        Tp::ContactPtr mContact;
        Tp::ConnectionPtr mConnection;
        QSharedPointer<QDBusPendingCallWatcher> mWatcher;
        QString mAccountPath;
        QString mAvatar;
        QString mAvatarMime;
        bool mHasCaps;
        bool mReady;
};

TpContact::TpContact(Tp::ContactPtr contact, QObject* parent) : QObject(parent)
    , d(new TpContactPrivate)
{

    d->mContact = contact;
    reqestFeatures(contact);

    connect(contact.data(),
            SIGNAL(capabilitiesChanged(Tp::ContactCapabilities*)),
            this, SLOT(onExtCapFinished(Tp::ContactCapabilities*)));

}

TpContact::TpContact(QObject * parent) : QObject(parent),
        d(new TpContactPrivate)
{
}

TpContact::~TpContact()
{
    delete d;
}


void TpContact::reqestFeatures(Tp::ContactPtr contact)
{
    QSet<Tp::Contact::Feature> feature;
    feature << Tp::Contact::FeatureAlias
        << Tp::Contact::FeatureAvatarToken
        << Tp::Contact::FeatureSimplePresence
        << Tp::Contact::FeatureCapabilities;

    Tp::PendingContacts * pc = contact->manager()->upgradeContacts (QList<Tp::ContactPtr>()<<contact, feature);

    connect(pc,
               SIGNAL(finished(Tp::PendingOperation *)),
               SLOT(onFeaturesReady(Tp::PendingOperation *)));
}

void TpContact::requestAvatar(Tp::ContactPtr contact)
{
    if (contact->manager()->connection()->avatarsInterface() == 0) {
        return;
    }

    Tp::UIntList tokenRequest;
    tokenRequest.append(contact->handle()[0]);
    d->mConnection =  contact->manager()->connection();
    d->mConnection->avatarsInterface()->RequestAvatars(tokenRequest);

    connect(d->mConnection->avatarsInterface(),
            SIGNAL(AvatarRetrieved(uint, const QString&, const QByteArray&, const QString&)),
            this,
            SLOT( onAvatarRetrieved(uint, const QString&, const QByteArray&, const QString&)));
    connect(d->mConnection->avatarsInterface(),
            SIGNAL(AvatarUpdated(uint, const QString&)),
            this,
            SLOT(onAvatarUpdated(uint, const QString&)));

}

void TpContact::onFeaturesReady(Tp::PendingOperation * op)
{
    Tp::PendingContacts * pa = qobject_cast<Tp::PendingContacts *>(op);
    Tp::ContactPtr contact = pa->contacts().at(0);

    if (contact) {
        d->mReady = true;
        d->mHasCaps = true;
        if (contact->actualFeatures().contains(Tp::Contact::FeatureAvatarToken)) {
           requestAvatar(contact);
        } else {
            qDebug() << Q_FUNC_INFO << "contact: Does not support avatars";
        }
        Q_EMIT ready(this);

        connect(contact.data(),
            SIGNAL(simplePresenceChanged(const QString &, uint, const QString &)),
            this, SLOT(onSimplePresenceChanged(const QString &, uint, const QString &)));

    }
}

void TpContact::onSimplePresenceChanged(const QString & status , uint stat, const QString & message)
{
    Q_UNUSED(stat)
    Q_UNUSED(status)
    Q_UNUSED(message)

    Q_EMIT change(uniqueId(), SIMPLE_PRESENCE);
}

void TpContact::onAvatarRetrieved(uint contact, const QString& token,
        const QByteArray& data, const QString& mime)
{
    Q_UNUSED(mime);

    if( d->mContact->handle()[0] == contact )
    {   //every contact receives signals for every avatar retrieved, have to distinguish
        //which one is for us
        QString extension =  mime.split("/").value(1); //sometimes its an empty mime, using .value
        QImage Avatar;
        d->mAvatarMime = extension;
        d->mAvatar = token;
        if (Avatar.loadFromData(data))
        {
            Avatar.save(::FilePath(token, extension));
        }
        Q_EMIT change(uniqueId(), AVATAR_TOKEN);
    }

}

void TpContact::onAvatarUpdated (uint contact, const QString &newAvatarToken)
{
    if(d->mContact->handle()[0] == contact && newAvatarToken != avatar()){
       Tp::UIntList tokenRequest;
       tokenRequest.append(d->mContact->handle()[0]);
       d->mConnection =  d->mContact->manager()->connection();
       d->mConnection->avatarsInterface()->RequestAvatars(tokenRequest);
    }
}


QString TpContact::avatar() const
{
    return d->mAvatar;
}

void TpContact::onExtCapFinished(Tp::ContactCapabilities* caps)
{
    Q_UNUSED(caps);
    Q_EMIT change(uniqueId(), CAPABILITIES);
}

void TpContact::setAccountPath(const QString& paths)
{
    d->mAccountPath = paths;
}

QString TpContact::accountPath() const
{
    return d->mAccountPath;
}

Tp::ContactPtr TpContact::contact()
{
    return d->mContact;
}

//Not that libtelepathy-qt doesn't have a ContactConstPtr because it doesn't
//seem to really understand the use of const and (smart) pointers. const ContactPtr would not be the same.
QSharedPointer<const Tp::Contact> TpContact::contact() const
{
    return d->mContact;
}

unsigned int TpContact::uniqueId() const
{
    return buildUniqueId(this->accountPath(), d->mContact->id());
}

unsigned int TpContact::buildUniqueId(const QString& accountPath, const QString& imId)
{
        return qHash(accountPath + '!' +  imId);
}


QUrl TpContact::imAddress() const
{
    return buildImAddress(this->accountPath(), d->mContact->id());
}

QUrl TpContact::buildImAddress(const QString& accountPath, const QString& imId)
{
    return QUrl("telepathy:" + accountPath + '!' +  imId);
}

bool TpContact::isReady() const
{
    return d->mReady;
}

QString TpContact::avatarMime() const
{
    return d->mAvatarMime;
}
