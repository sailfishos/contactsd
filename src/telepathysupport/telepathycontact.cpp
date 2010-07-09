/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact:  Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */


#include "telepathycontact.h"
#include <QtCore>
#include <QImage>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/PendingContacts>
#include "contactcache.h"
#include <contactphotocopy.h>

/*!
 * utility method just for usage here
 */
const QString avatarFilePath(QString avatarToken, QString extension)
{
    return ContactPhotoCopy::avatarDir()+"/telepathy_cache"+avatarToken+'.'+extension;
}

TelepathyContact::TelepathyContact(Tp::ContactPtr contact, int groupID) : QObject(0)
    , mContact(contact)
    , mGroupID(groupID)
    , mCapabilities(0)
    , mHasStorageId(false)
    , mStorageId(-1)
    , _uniqueId(0)
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


    setId(contact->id());
    Tp::UIntList tokenRequest;
    tokenRequest.append(contact->handle()[0]);
    mConnection =  contact->manager()->connection();
    mConnection->avatarsInterface()->RequestAvatars(tokenRequest);
    connect(mConnection->avatarsInterface(),
            SIGNAL(AvatarRetrieved(uint, const QString&, const QByteArray&, const QString&)),
            this,
            SLOT( onAvatarRetrieved(uint, const QString&, const QByteArray&, const QString&)));
    connect(mConnection->avatarsInterface(),
            SIGNAL(AvatarUpdated(uint, const QString&)),
            this,
            SLOT(onAvatarUpdated(uint, const QString&)));
    connect(contact.data(),
            SIGNAL(simplePresenceChanged(const QString &, uint, const QString &)),
            this, SLOT(onSimplePresenceChanged(const QString &, uint, const QString &)));

    connect(contact.data(),
            SIGNAL(capabilitiesChanged(Tp::ContactCapabilities*)),
            this, SLOT(onExtCapFinished(Tp::ContactCapabilities*)));


}

TelepathyContact::TelepathyContact(QObject * parent) : QObject(parent),
        mContact(0), mGroupID(0), mType(0), mCapabilities(0), _uniqueId(0)
{
}

void TelepathyContact::onFeaturesReady(Tp::PendingOperation * op)
{
    Tp::PendingContacts * pa = qobject_cast<Tp::PendingContacts *>(op);
    Tp::ContactPtr contact = pa->contacts().at(0);

    if (contact) {
    setAlias(contact->alias());
    setMessage(contact->presenceMessage());
    setType(contact->presenceType());
    setPresenceStatus(contact->presenceStatus());
    qDebug() <<  Q_FUNC_INFO << ':' << contact->alias() << ": " ;
    mCapabilities = contact->capabilities();
    qDebug() << Q_FUNC_INFO << "Capabilities " << mCapabilities->supportsTextChats();
    emit simplePresenceChanged(this, contact->presenceStatus(), contact->presenceType(), contact->presenceMessage(), mGroupID);
    Q_EMIT featuresReady(this);
    }
}

TelepathyContact::~TelepathyContact()
{
    if (mCapabilities) {
        delete mCapabilities;
    }
}

void TelepathyContact::onSimplePresenceChanged(const QString & status , uint stat, const QString & message)
{
    setMessage(message);
    setType(stat);
    /*
    qDebug() << Q_FUNC_INFO <<
        "\nContact Id : " << id() <<
        "\nStat : " << stat <<
        "\nMessage : " << message <<
        "\nStatus" << status;
    */
    emit simplePresenceChanged(this, status, stat, message, mGroupID);
}

void TelepathyContact::onAvatarRetrieved(uint contact, const QString& token,
        const QByteArray& data, const QString& mime)
{
    Q_UNUSED(mime);

    if( mContact->handle()[0] == contact )
    {   //every contact receives signals for every avatar retrieved, have to distinguish
        //which one is for us
        const QString extension =  mime.split('/').value(1); //sometimes its an empty mime, using .value
        qDebug() <<  Q_FUNC_INFO << mime << extension;
        QImage mAvatar;
        if (mAvatar.loadFromData(data))
        {
            mAvatar.save(::avatarFilePath(token, extension));
        }
        setAvatarToken(token, extension);
    }
}

void TelepathyContact::onAvatarUpdated (uint contact, const QString &newAvatarToken)
{
    if(mContact->handle()[0] == contact && newAvatarToken != avatar()){
       Tp::UIntList tokenRequest;
       tokenRequest.append(mContact->handle()[0]);
       mConnection =  mContact->manager()->connection();
       mConnection->avatarsInterface()->RequestAvatars(tokenRequest);
    }
}

void TelepathyContact::setId(const QString& Id)
{
       mId = Id;
}

QString TelepathyContact::id( ) const
{
    return mId;
}

void TelepathyContact::setAlias(const QString& Alias)
{
    mAlias = Alias;
}
QString TelepathyContact::alias() const
{
        return mAlias;
}

void TelepathyContact::setType(int aType)
{
        mType = aType;
}

int TelepathyContact::type() const
{
       return mType;
}

void TelepathyContact::setMessage(const QString& aMessage)
{
       mMessage = aMessage;
}

QString TelepathyContact::message()
{
       return mMessage;
}

void TelepathyContact::setPresenceStatus(const QString& status)
{
    mStatusString = status;
}

QString TelepathyContact::presenceStatus() const
{
    return mStatusString;
}

void TelepathyContact::setAvatarToken(const QString& avatar, const QString& mime)
{
  if(mAvatar != avatar) {
   mAvatar = avatar;
   QMetaObject::invokeMethod(this, "avatarReady",Qt::QueuedConnection);
   QMetaObject::invokeMethod(this, "avatarReady",Qt::QueuedConnection,
                             Q_ARG(QString, id()), Q_ARG(QString, avatar),
                             Q_ARG(QString, mime));
   }
}

QString TelepathyContact::avatar() const
{
    return mAvatar;
}

QString TelepathyContact::avatarFilePath() const
{
    //TODO merge in Siraj's changes about avatar path
    return mAvatar.isEmpty()? QString() : ::avatarFilePath(mAvatar, "jpeg");
}

void TelepathyContact::requestCapabilities()
{

   if (mConnection->interfaces().contains(TELEPATHY_INTERFACE_CONNECTION_INTERFACE_CAPABILITIES)) {

        QDBusPendingReply<Tp::ContactCapabilityList> reply =
                mConnection->capabilitiesInterface()->GetCapabilities(Tp::UIntList() << mContact->handle()[0]);

        mWatcher = QSharedPointer<QDBusPendingCallWatcher> (new QDBusPendingCallWatcher(reply),
                   &QObject::deleteLater);

        connect(mWatcher.data(), SIGNAL(finished(QDBusPendingCallWatcher*)),
                this, SLOT(onCapFinished(QDBusPendingCallWatcher*)));
   }

}

void TelepathyContact::onExtCapFinished(Tp::ContactCapabilities* caps)
{
    qDebug() << Q_FUNC_INFO <<  "Extended capabilities";
    mCapabilities = caps;
}


void TelepathyContact::onCapFinished(QDBusPendingCallWatcher * self )
{

    QDBusPendingReply<Tp::ContactCapabilityList> reply = *self;

    if(reply.isError()) {
        qDebug() << "Error Getting Cababilities of contact";
        Tp::ContactCapabilityList empty;
        emit capabilities(this, empty);
        return ;
    }
    /*
    foreach(Tp::ContactCapability cap, reply.value()) {
       qDebug() << cap.channelType;
    }
    */
    mCaps = reply.value();
    emit capabilities(this, reply.value());
}

void TelepathyContact::setAccountPath(const QString& paths)
{
    mAccountPath = paths;
}

QString TelepathyContact::accountPath() const
{
    return mAccountPath;
}

Tp::ContactPtr TelepathyContact::contact()
{
    return mContact;
}

Tp::ContactCapabilityList TelepathyContact::capabilities() const
{
    return mCaps;
}

bool TelepathyContact::hasCapabilities()
{
    if (mCaps.count() <= 0 ) {
        return false;
    }

    return true;
}

bool TelepathyContact::hasContactCapabilities()
{
    if (mCapabilities) {
        return true;
    }

    return false;
}
bool TelepathyContact::hasStorageId()
{
    return mHasStorageId;
}

void TelepathyContact::requestStorageId()
{

}

uint TelepathyContact::storageId() const
{
    return mStorageId;
}

Tp::ContactCapabilities* TelepathyContact::contactCapabilities() const
{
    return mCapabilities;
}

unsigned int TelepathyContact::uniqueId() const
{
    if( _uniqueId ) //used for tests
        return _uniqueId;
    else
        return qHash(accountPath() + mContact->id());
}
