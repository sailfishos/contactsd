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

#include "cdtpplugin.h"

#include "cdtpcontroller.h"
#include "cdtppendingrosters.h"
#include "cdtptrackersink.h"

#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/QLive>
#include <QtTracker/Tracker>

using namespace SopranoLive;

CDTpPlugin::CDTpPlugin():
        m_tpController( new CDTpController( this, false ) ),
        mStore(CDTpTrackerSink::instance()),
        mImportActive(false)
{}

CDTpPlugin::~CDTpPlugin()
{
    delete m_tpController;
    qDeleteAll(mAccounts);
    mAccounts.clear();
}


void CDTpPlugin::init()
{
    qDebug() << Q_FUNC_INFO << "Initializing CDTpPlugin.";
    mAm = Tp::AccountManager::create();
    connect(mAm->becomeReady(), SIGNAL(finished(Tp::PendingOperation*)),
            this,SLOT(onAccountManagerReady(Tp::PendingOperation*)));
    connect(mAm.data(),
            SIGNAL(accountCreated(const QString &)),
            SLOT(onAccountCreated(const QString &)));
    accountServiceMapper.initialize();
}

QMap<QString, QVariant> CDTpPlugin::metaData()
{
    QMap<QString, QVariant> data;
    data["name"]= QVariant(QString("telepathy"));
    data["version"] = QVariant(QString("0.1"));
    data["comment"] = QVariant(QString("Telepathy Contact Collector"));
    //TODO: translations ?
    return data;
}

void CDTpPlugin::onAccountCreated(const QString& path)
{
    Tp::AccountPtr account = Tp::Account::create(mAm->busName(), path);
    CDTpPendingRosters* request = m_tpController->requestRosters(account);
    mRosters.append(request);

    connect(request, SIGNAL(finished(CDTpPendingRosters *)),
            this,    SLOT(onFinished(CDTpPendingRosters *)));
    Tp::Features features;
    features << Tp::Account::FeatureAvatar
        << Tp::Account::FeatureCore
        << Tp::Account::FeatureProtocolInfo;
    connect(account->becomeReady(features), SIGNAL(finished(Tp::PendingOperation*)),
            this, SLOT(onAccountReady(Tp::PendingOperation*)));

}

void CDTpPlugin::onAccountManagerReady(Tp::PendingOperation* op)
{
    qDebug() << Q_FUNC_INFO << "Account manager ready.";

    if (op->isError() ) {
        qDebug() << Q_FUNC_INFO << ": Error: " << op->errorMessage() << " - " << op->errorName();
        return;
    }

    foreach (Tp::AccountPtr account, mAm->validAccounts() ) {
        CDTpPendingRosters* request = m_tpController->requestRosters(account);
        mRosters.append(request);
        connect(request, SIGNAL(finished(CDTpPendingRosters *)),
                this,    SLOT(onFinished(CDTpPendingRosters *)));
        
     Tp::Features features;
     features << Tp::Account::FeatureAvatar
        << Tp::Account::FeatureCore
        << Tp::Account::FeatureProtocolInfo;

        connect(account->becomeReady(features), SIGNAL(finished(Tp::PendingOperation*)),
                this, SLOT(onAccountReady(Tp::PendingOperation*)));
    }
}

void CDTpPlugin::onAccountReady(Tp::PendingOperation* op)
{
    if (op->isError()) {
        return;
    }

    Tp::PendingReady * pa = qobject_cast<Tp::PendingReady *>(op);
    Tp::AccountPtr account = Tp::AccountPtr(qobject_cast<Tp::Account *>(pa->object()));

    if (!account) {
        return;
    }

    qDebug() << Q_FUNC_INFO << ": Account ready: " << account->objectPath();

    CDTpAccount * tpaccount = new CDTpAccount(account);
    connect(tpaccount, SIGNAL(accountChanged(CDTpAccount*,CDTpAccount::Changes)),
            this, SLOT(onAccountChanged(CDTpAccount*,CDTpAccount::Changes)));
    mAccounts.append(tpaccount);
    saveSelfContact(account);
    
    connect(account.data(), SIGNAL(removed()), this, SLOT(onAccountRemoved()));
    connect(account.data(), SIGNAL(onlinenessChanged(bool)), this, SLOT(onOnlinenessChanged(bool)));
    connect(account.data(), SIGNAL(haveConnectionChanged(bool)), this, SLOT(onConnectionChanged(bool)));
}

void CDTpPlugin::onConnectionChanged(bool connection)
{
    qDebug() << Q_FUNC_INFO << connection;
    if (!connection) {
        Tp::Account*  account = qobject_cast<Tp::Account *>(sender());
        if (account) {
            mStore->takeAllOffline(account->objectPath());
        }
    }
}
void CDTpPlugin::onOnlinenessChanged(bool online)
{
    qDebug() << Q_FUNC_INFO << online;
    if (!online) {
        
        Tp::Account*  account = qobject_cast<Tp::Account *>(sender());
        if (account) {
            if (!account->isEnabled()) {
                mStore->deleteContacts(account->objectPath());
            }
        }
    }
}
void CDTpPlugin::onAccountRemoved()
{
    if (sender()) {
        Tp::Account*  account = qobject_cast<Tp::Account *>(sender());
        if (account) {
            qDebug() << Q_FUNC_INFO << "Account Removed:"<<  account->objectPath();
            mStore->deleteContacts(account->objectPath());
        }
    }
}

void CDTpPlugin::onFinished(CDTpPendingRosters* op)
{
    qDebug() << Q_FUNC_INFO << ": Request roster operation finished.";

    if (op->isError() ) {
        emit error("libtelepathycollectorplugin", op->errorName(), op->errorMessage());
    }
    
    CDTpPendingRosters * roster = qobject_cast<CDTpPendingRosters*>(op);
    emit importStarted();
    mImportActive = true;
    foreach (QSharedPointer<CDTpContact> c, roster->telepathyRosterList()) {
        qDebug() << Q_FUNC_INFO << "Adding Contacts";
        mStore->sinkToStorage(c);
    }

    emit importEnded(roster->telepathyRosterList().count(), 0, 0);
    mImportActive = false;

    connect(roster, SIGNAL(contactsAdded(QList<QSharedPointer<CDTpContact> >)), this, SLOT(onContactsAdded(QList<QSharedPointer<CDTpContact> >)));
    connect(roster, SIGNAL(contactsRemoved(QList<QSharedPointer<CDTpContact> >)), this, SLOT(onContactsRemoved(QList<QSharedPointer<CDTpContact> >)));
    
}

void CDTpPlugin::onContactsRemoved(QList<QSharedPointer<CDTpContact> > list)
{
   for (int i = 0 ; i < list.count() ; i++) {
       QSharedPointer<CDTpContact> contact = list.value(i);
       if (contact) {
           if (contact.data()) {
               mStore->deleteContact(contact);
           }
       }
   }
}

void CDTpPlugin::onContactsAdded(QList<QSharedPointer<CDTpContact> > list)
{
    qDebug() << Q_FUNC_INFO << list.count();
   for (int i = 0 ; i < list.count() ; i++) {
       QSharedPointer<CDTpContact> contact = list.value(i);
       if (contact) {
           if (contact.data()) {
               mStore->sinkToStorage(contact);
           }
       }
   }
}

void CDTpPlugin::onContactsChanged(const Tp::Contacts& contactsAdded, const Tp::Contacts& contactsRemoved)
{
    foreach (const Tp::ContactPtr contact, contactsAdded) {
        QSharedPointer<CDTpContact> tpcontact = QSharedPointer<CDTpContact>(new CDTpContact(contact), &QObject::deleteLater);
        mStore->sinkToStorage(tpcontact);
    }

    foreach (const Tp::ContactPtr contact, contactsRemoved) {

    }
}

void CDTpPlugin::onAccountChanged(CDTpAccount* account, CDTpAccount::Changes changes)
{
    qDebug() << Q_FUNC_INFO << ": account " << *account << "changed: " << changes;

    saveIMAccount(account->account(), changes);

    const Tp::SimplePresence presence (account->account()->currentPresence());

    if (changes == CDTpAccount::Presence && presence.status != "offline") {
        qDebug() << Q_FUNC_INFO << "Presence Change";
        mStore->clearContacts(account->account()->objectPath());
        CDTpPendingRosters* request = m_tpController->requestRosters(account->account());
        mRosters.append(request);
        connect(request, SIGNAL(finished(Tp::PendingOperation *)),
                this,    SLOT(onFinished(Tp::PendingOperation *)));

       connect(request, SIGNAL(contactsAdded(QList<QSharedPointer<CDTpContact> >)), this, SLOT(onContactsAdded(QList<QSharedPointer<CDTpContact> >)));
       connect(request, SIGNAL(contactsRemoved(QList<QSharedPointer<CDTpContact> >)), this, SLOT(onContactsRemoved(QList<QSharedPointer<CDTpContact> >)));
    

    }
}
//TODO
//remove this ?
void CDTpPlugin::saveSelfContact(Tp::AccountPtr account)
{
    qDebug() << Q_FUNC_INFO << ": Saving self contact to Tracker. Account is: " << account->objectPath();
    saveIMAccount(account, CDTpAccount::All);
}

void CDTpPlugin::saveIMAccount(Tp::AccountPtr account, CDTpAccount::Changes changes)
{
    qDebug() << Q_FUNC_INFO << ": saving own IM account: " << account->objectPath() << "with changed: " << changes;


    QUrl accountUrl("telepathy:" + account->objectPath());
    RDFVariable theAccount = RDFVariable::fromType<nco::IMAccount>();
    Live<nco::IMAccount> liveAccount = ::tracker()->liveNode(accountUrl);

    RDFUpdate up;
    up.addInsertion(theAccount, nco::IMAccount::iri(), liveAccount.variable());
    ::tracker()->executeQuery(up);

    accountModelReady(account);
}

// TODO duplicated logic from above (saveIMAccount):
// creating LiveNode then RDFUpdate then again LiveNode... once
// instead of 3 times should work
void CDTpPlugin::accountModelReady(Tp::AccountPtr account)
{

     Live<nco::IMAccount> liveAccount = ::tracker()->liveNode(QUrl("telepathy:"+account->objectPath()));

     QString displayName = accountServiceMapper.serviceForAccountPath(account->objectPath());

     if (displayName.isEmpty()) {
         liveAccount->setImDisplayName(account->displayName());
     } else {
         liveAccount->setImDisplayName(accountServiceMapper.serviceForAccountPath(account->objectPath()));
     }

     liveAccount->setImAccountType(account->protocol());

     const QUrl imAddressUrl("telepathy:"+account->objectPath() + "!"
                            + account->parameters()["account"].toString());
     Live<nco::IMAddress> addressInfo =
         ::tracker()->liveNode(imAddressUrl);

     addressInfo->addImID(account->parameters()["account"].toString());
     addressInfo->setImNickname(account->nickname());
     const QDateTime datetime = QDateTime::currentDateTime();

     addressInfo->setPresenceLastModified(datetime);
     liveAccount->addImAccountAddress(addressInfo);

     Tp::SimplePresence presence = account->currentPresence();
     addressInfo->setImStatusMessage(presence.statusMessage);
     Live<nco::PresenceStatus> cstatus =
     ::tracker()->liveNode(CDTpTrackerSink::toTrackerStatus(presence.status));
     addressInfo->setImPresence(cstatus);
     //link the IMAddress to me-contact
     Live<nco::PersonContact> me = ::tracker()->liveResource<nco::default_contact_me>();
     //save avatar
     QString filename;
     bool ok = saveAvatar(account->avatar().avatarData, account->avatar().MIMEType, QDir::homePath()
             + "/.contacts/avatars/", filename);

    if (ok) {
        Live<nie::DataObject> fileUrl = ::tracker()->liveNode(QUrl(filename));
        addressInfo->addImAvatar(fileUrl);  
        Live<nie::DataObject> photo = me->getPhoto();
        if (photo->getUrl().isEmpty()) {
            me->setPhoto(fileUrl);
        }
    }
    
     me->addHasIMAddress(addressInfo);
     // Writing local UID for default-contact-me
     const QString strLocalUID = QString::number(0x7FFFFFFF);
     me->setContactLocalUID(strLocalUID);

     if (presence.status == "offline") {
         mStore->takeAllOffline(account->objectPath());
     }

}

bool CDTpPlugin::saveAvatar(const QByteArray& data, const QString& mime, const QString& path,
                                 QString& fileName )
{
    qDebug() << Q_FUNC_INFO << ": Saving avatar image: " << fileName;

    // if image data is empty, lets leave
    if (data.size() == 0) {
        qWarning() << Q_FUNC_INFO << ": Empty avatar image.";
        return false;
    }

    const QString ext = mime.split('/').value(1);
    QImage img;
    img.loadFromData(data);
    QString file = path + QString(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());

    // check if mime was
    if (ext.isEmpty()) {
        fileName = file + ".jpeg";
    }
    else {
        fileName = file + '.' + ext;
    }

    if (img.save(fileName) == false) {
        qWarning() << Q_FUNC_INFO << ": Saving avatar image failed: " << fileName;
        return false;
    }

    qDebug() << Q_FUNC_INFO << ": Avatar image saved successfully: " << fileName;

    return true;
}

bool CDTpPlugin::hasActiveImports()
{
    return mImportActive;
}

Q_EXPORT_PLUGIN2(TpPlugin, CDTpPlugin)
