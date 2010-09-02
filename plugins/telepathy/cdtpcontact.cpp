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

#include "cdtpcontact.h"

#include "cdtpcontactphotocopy.h"

#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/Types>

#include <QImage>
#include <QtCore>

/*!
 * utility method just for usage here
 */
const QString FilePath(QString avatarToken, QString extension)
{
    return CDTpContactPhotoCopy::avatarDir()+"/telepathy_cache"+avatarToken+'.'+extension;
}

class CDTpContact::CDTpContactPrivate
{
    public:
    CDTpContactPrivate() :
            mHasCaps(false),
            mReady(false) {}
    ~CDTpContactPrivate() {}

        Tp::ContactPtr mContact;
        Tp::ConnectionPtr mConnection;
        QSharedPointer<QDBusPendingCallWatcher> mWatcher;
        QString mAccountPath;
        QString mAvatar;
        QString mAvatarMime;
        bool mHasCaps;
        bool mReady;
};

CDTpContact::CDTpContact(Tp::ContactPtr contact, QObject* parent) : QObject(parent)
    , d(new CDTpContactPrivate)
{

    d->mContact = contact;
    reqestFeatures(contact);

    connect(contact.data(),
            SIGNAL(capabilitiesChanged(Tp::ContactCapabilities*)),
            this, SLOT(onExtCapFinished(Tp::ContactCapabilities*)));

}

CDTpContact::CDTpContact(QObject * parent) : QObject(parent),
        d(new CDTpContactPrivate)
{
}

CDTpContact::~CDTpContact()
{
    delete d;
}


void CDTpContact::reqestFeatures(Tp::ContactPtr contact)
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

void CDTpContact::requestAvatar(Tp::ContactPtr contact)
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

void CDTpContact::onFeaturesReady(Tp::PendingOperation * op)
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

void CDTpContact::onSimplePresenceChanged(const QString & status , uint stat, const QString & message)
{
    Q_UNUSED(stat)
    Q_UNUSED(status)
    Q_UNUSED(message)

    Q_EMIT change(uniqueId(), SIMPLE_PRESENCE);
}

void CDTpContact::onAvatarRetrieved(uint contact, const QString& token,
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

void CDTpContact::onAvatarUpdated (uint contact, const QString &newAvatarToken)
{
    if(d->mContact->handle()[0] == contact && newAvatarToken != avatar()){
       Tp::UIntList tokenRequest;
       tokenRequest.append(d->mContact->handle()[0]);
       d->mConnection =  d->mContact->manager()->connection();
       d->mConnection->avatarsInterface()->RequestAvatars(tokenRequest);
    }
}


QString CDTpContact::avatar() const
{
    return d->mAvatar;
}

void CDTpContact::onExtCapFinished(Tp::ContactCapabilities* caps)
{
    Q_UNUSED(caps);
    Q_EMIT change(uniqueId(), CAPABILITIES);
}

void CDTpContact::setAccountPath(const QString& paths)
{
    d->mAccountPath = paths;
}

QString CDTpContact::accountPath() const
{
    return d->mAccountPath;
}

//Not that libtelepathy-qt doesn't have a ContactConstPtr because it doesn't
//seem to really understand the use of const and (smart) pointers. const ContactPtr would not be the same.
QSharedPointer<const Tp::Contact> CDTpContact::contact() const
{
    return d->mContact;
}

QString CDTpContact::id() const
{
    return d->mContact->id();
}

QString CDTpContact::alias() const
{
    return d->mContact->alias();
}

unsigned int CDTpContact::presenceType() const
{
    return d->mContact->presenceType();
}

QString CDTpContact::presenceMessage() const
{
    return d->mContact->presenceMessage();
}

Tp::ContactCapabilities *CDTpContact::capabilities() const
{
    return d->mContact->capabilities();
}

bool CDTpContact::supportsTextChats() const
{
    const Tp::ContactCapabilities *caps = capabilities();
    return caps && caps->supportsTextChats();
}

bool CDTpContact::supportsMediaCalls() const
{
    const Tp::ContactCapabilities *caps = capabilities();
    return caps && caps->supportsMediaCalls();
}

bool CDTpContact::supportsAudioCalls() const
{
    const Tp::ContactCapabilities *caps = capabilities();
    return caps && caps->supportsAudioCalls();
}

bool CDTpContact::supportsVideoCalls(bool withAudio) const
{
    const Tp::ContactCapabilities *caps = capabilities();
    return caps && caps->supportsVideoCalls(withAudio);
}

bool CDTpContact::supportsUpgradingCalls() const
{
    const Tp::ContactCapabilities *caps = capabilities();
    return caps && caps->supportsUpgradingCalls();
}


unsigned int CDTpContact::uniqueId() const
{
    return buildUniqueId(this->accountPath(), id());
}

unsigned int CDTpContact::buildUniqueId(const QString& accountPath, const QString& imId)
{
        return qHash(accountPath + '!' +  imId);
}

QUrl CDTpContact::imAddress() const
{
    return buildImAddress(this->accountPath(), id());
}

QUrl CDTpContact::buildImAddress(const QString& accountPath, const QString& imId)
{
    return QUrl("telepathy:" + accountPath + '!' +  imId);
}

bool CDTpContact::isReady() const
{
    return d->mReady;
}

QString CDTpContact::avatarMime() const
{
    return d->mAvatarMime;
}
