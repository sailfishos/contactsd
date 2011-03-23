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

#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountPropertyFilter>
#include <TelepathyQt4/AndFilter>
#include <TelepathyQt4/NotFilter>
#include <TelepathyQt4/AccountSet>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingContacts>

#include "buddymanagementadaptor.h"
#include "cdtpcontroller.h"
#include "cdtpstorage.h"
#include "debug.h"

using namespace Contactsd;

const QLatin1String DBusObjectPath("/telepathy");

CDTpController::CDTpController(QObject *parent) : QObject(parent)
{
    debug() << "Creating storage";
    mStorage = new CDTpStorage(this);
    mOfflineRosterBuffer = new QSettings(QSettings::IniFormat,
            QSettings::UserScope, QLatin1String("Nokia"),
            QLatin1String("Contactsd"));
    connect(mStorage,
            SIGNAL(syncStarted(CDTpAccountPtr)),
            SLOT(onSyncStarted(CDTpAccountPtr)));
    connect(mStorage,
            SIGNAL(syncEnded(CDTpAccountPtr, int, int)),
            SLOT(onSyncEnded(CDTpAccountPtr, int, int)));

    debug() << "Creating account manager";
    const QDBusConnection &bus = QDBusConnection::sessionBus();
    Tp::AccountFactoryPtr accountFactory = Tp::AccountFactory::create(bus,
            Tp::Features() << Tp::Account::FeatureCore
                           << Tp::Account::FeatureAvatar
                           << Tp::Account::FeatureCapabilities);
    Tp::ConnectionFactoryPtr connectionFactory = Tp::ConnectionFactory::create(bus,
            Tp::Features() << Tp::Connection::FeatureCore
                           << Tp::Connection::FeatureRoster);
    Tp::ChannelFactoryPtr channelFactory = Tp::ChannelFactory::create(bus);
    Tp::ContactFactoryPtr contactFactory = Tp::ContactFactory::create(
            Tp::Features() << Tp::Contact::FeatureAlias
                           << Tp::Contact::FeatureAvatarToken
                           << Tp::Contact::FeatureAvatarData
                           << Tp::Contact::FeatureSimplePresence
                           << Tp::Contact::FeatureInfo
                           << Tp::Contact::FeatureLocation
                           << Tp::Contact::FeatureCapabilities);
    mAM = Tp::AccountManager::create(bus, accountFactory, connectionFactory,
            channelFactory, contactFactory);

    // Wait for AM to become ready
    connect(mAM->becomeReady(Tp::AccountManager::FeatureCore),
            SIGNAL(finished(Tp::PendingOperation*)),
            SLOT(onAccountManagerReady(Tp::PendingOperation*)));
    if (registerDBusObject()) {
        (void) new BuddyManagementAdaptor(this);
    }
}

CDTpController::~CDTpController()
{
    QDBusConnection::sessionBus().unregisterObject(DBusObjectPath);
    if (mOfflineRosterBuffer) {
        delete mOfflineRosterBuffer;
    }
}

void CDTpController::onAccountManagerReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        debug() << "Could not make account manager ready:" <<
            op->errorName() << "-" << op->errorMessage();
        return;
    }

    debug() << "Account manager ready";

    Tp::AccountPropertyFilterPtr filter1 = Tp::AccountPropertyFilter::create();
    filter1->addProperty(QString::fromLatin1("valid"), true);
    filter1->addProperty(QString::fromLatin1("hasBeenOnline"), true);

    Tp::AccountPropertyFilterPtr normalizedNameFilter = Tp::AccountPropertyFilter::create();
    normalizedNameFilter->addProperty(QString::fromLatin1("normalizedName"), QString());
    Tp::AccountFilterPtr filter2 = Tp::NotFilter<Tp::Account>::create(normalizedNameFilter);

    Tp::AccountFilterPtr filter = Tp::AndFilter<Tp::Account>::create(
            QList<Tp::AccountFilterConstPtr>() << filter1 << filter2);

    mAccountSet = mAM->filterAccounts(filter);
    connect(mAccountSet.data(),
            SIGNAL(accountAdded(const Tp::AccountPtr &)),
            SLOT(onAccountAdded(const Tp::AccountPtr &)));
    connect(mAccountSet.data(),
            SIGNAL(accountRemoved(const Tp::AccountPtr &)),
             SLOT(onAccountRemoved(const Tp::AccountPtr &)));

    Q_FOREACH (const Tp::AccountPtr &account, mAccountSet->accounts()) {
        insertAccount(account, false);
    }

    mStorage->syncAccounts(mAccounts.values());
}

void CDTpController::onAccountAdded(const Tp::AccountPtr &account)
{
    if (mAccounts.contains(account->objectPath())) {
        warning() << "Internal error, account was already in controller";
        return;
    }

    CDTpAccountPtr accountWrapper = insertAccount(account, true);
    mStorage->syncAccount(accountWrapper);
}

void CDTpController::onAccountRemoved(const Tp::AccountPtr &account)
{
    CDTpAccountPtr accountWrapper(mAccounts.take(account->objectPath()));
    if (not accountWrapper) {
        warning() << "Internal error, account was not in controller";
        return;
    }
    mStorage->removeAccount(accountWrapper);
}

CDTpAccountPtr CDTpController::insertAccount(const Tp::AccountPtr &account, bool newAccount)
{
    debug() << "Creating wrapper for account" << account->objectPath();

    // Get the list of contact ids waiting to be removed from server
    mOfflineRosterBuffer->beginGroup(QLatin1String("OfflineRemovals"));
    QStringList idsToRemove = mOfflineRosterBuffer->value(account->objectPath()).toStringList();
    mOfflineRosterBuffer->endGroup();

    CDTpAccountPtr accountWrapper = CDTpAccountPtr(new CDTpAccount(account, idsToRemove, newAccount, this));
    mAccounts.insert(account->objectPath(), accountWrapper);

    // Start removal operation
    if (!idsToRemove.isEmpty()) {
        PendingOfflineRemoval *removalPending = new PendingOfflineRemoval(accountWrapper, idsToRemove, this);
        connect(removalPending,
                SIGNAL(finished(PendingOfflineRemoval *)),
                SLOT(onRemovalFinished(PendingOfflineRemoval *)));
    }

    // Connect change notifications
    connect(accountWrapper.data(),
            SIGNAL(changed(CDTpAccountPtr, CDTpAccount::Changes)),
            mStorage,
            SLOT(updateAccount(CDTpAccountPtr, CDTpAccount::Changes)));
    connect(accountWrapper.data(),
            SIGNAL(rosterChanged(CDTpAccountPtr)),
            mStorage,
            SLOT(syncAccountContacts(CDTpAccountPtr)));
    connect(accountWrapper.data(),
            SIGNAL(rosterUpdated(CDTpAccountPtr,
                    const QList<CDTpContactPtr> &,
                    const QList<CDTpContactPtr> &)),
            mStorage,
            SLOT(syncAccountContacts(CDTpAccountPtr,
                    const QList<CDTpContactPtr> &,
                    const QList<CDTpContactPtr> &)));
    connect(accountWrapper.data(),
            SIGNAL(rosterContactChanged(CDTpContactPtr, CDTpContact::Changes)),
            mStorage,
            SLOT(updateContact(CDTpContactPtr, CDTpContact::Changes)));

    return accountWrapper;
}

void CDTpController::onSyncStarted(CDTpAccountPtr accountWrapper)
{
    Tp::AccountPtr account = accountWrapper->account();
    Q_EMIT importStarted(accountWrapper->account()->serviceName(), account->objectPath());
}

void CDTpController::onSyncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved)
{
    Tp::AccountPtr account = accountWrapper->account();
    Q_EMIT importEnded(accountWrapper->account()->serviceName(), account->objectPath(),
        contactsAdded, contactsRemoved, 0);
}

void CDTpController::inviteBuddy(const QString &accountPath, const QString &imId)
{
    inviteBuddies(accountPath, QStringList() << imId);
}

void CDTpController::inviteBuddies(const QString &accountPath, const QStringList &imIds)
{
    debug() << "InviteBuddies:" << accountPath << imIds.join(QLatin1String(", "));

    CDTpAccountPtr accountWrapper(mAccounts.value(accountPath));
    if (not accountWrapper) {
        debug() << "Account not found";
        return;
    }

    if (accountWrapper->hasRoster()) {
        Tp::AccountPtr account = accountWrapper->account();
        Tp::ContactManagerPtr manager = account->connection()->contactManager();
        if (!manager->canRequestPresenceSubscription()) {
            debug() << "Account does not support subscribe";
            return;
        }
        Tp::PendingContacts *call = manager->contactsForIdentifiers(imIds);
        connect(call,
                SIGNAL(finished(Tp::PendingOperation *)),
                SLOT(onInviteContactRetrieved(Tp::PendingOperation *)));
    } else {
        // FIXME: Should we support offline invitations?
        debug() << "Account does not have a roster (offline?)";
    }
}

void CDTpController::onInviteContactRetrieved(Tp::PendingOperation *op)
{
    if (op->isError()) {
        return;
    }

    Tp::PendingContacts *pcontacts = qobject_cast<Tp::PendingContacts *>(op);
    QList<Tp::ContactPtr> contacts = pcontacts->contacts();
    if (contacts.isEmpty()) {
        return;
    }

    pcontacts->manager()->requestPresenceSubscription(contacts);
}

void CDTpController::removeBuddy(const QString &accountPath, const QString &imId)
{
    removeBuddies(accountPath, QStringList() << imId);
}

void CDTpController::removeBuddies(const QString &accountPath, const QStringList &imIds)
{
    debug() << "RemoveBuddies:" << accountPath << imIds.join(QLatin1String(", "));

    // Remove contact from storage
    mStorage->removeAccountContacts(accountPath, imIds);

    // Add contact to offlineRemovals, in case it does not get removed right now from server
    mOfflineRosterBuffer->beginGroup(QLatin1String("OfflineRemovals"));
    QStringList currentList = mOfflineRosterBuffer->value(accountPath).toStringList();
    Q_FOREACH (const QString &id, imIds) {
        if (!currentList.contains(id)) {
            currentList << id;
        }
    }
    mOfflineRosterBuffer->setValue(accountPath, currentList);
    mOfflineRosterBuffer->endGroup();
    mOfflineRosterBuffer->sync();

    CDTpAccountPtr accountWrapper = mAccounts[accountPath];
    if (!accountWrapper) {
        return;
    }

    // Add contact to account's avoid list
    accountWrapper->setContactsToAvoid(currentList);

    // Start removal operation
    PendingOfflineRemoval *removalPending = new PendingOfflineRemoval(accountWrapper, imIds, this);
    connect(removalPending,
            SIGNAL(finished(PendingOfflineRemoval *)),
            SLOT(onRemovalFinished(PendingOfflineRemoval *)));
}

void CDTpController::onRemovalFinished(PendingOfflineRemoval *op)
{
    debug() << "Contacts removed from server:" << op->contactIds().join(QLatin1String(", "));

    CDTpAccountPtr accountWrapper = op->accountWrapper();
    const QString accountPath = accountWrapper->account()->objectPath();

    // Remove contacts from the OfflineRemovals
    mOfflineRosterBuffer->beginGroup(QLatin1String("OfflineRemovals"));
    QStringList currentList = mOfflineRosterBuffer->value(accountPath).toStringList();
    Q_FOREACH (const QString id, op->contactIds()) {
        currentList.removeOne(id);
    }
    if (currentList.isEmpty()) {
        mOfflineRosterBuffer->remove(accountPath);
    } else {
        mOfflineRosterBuffer->setValue(accountPath, currentList);
    }
    mOfflineRosterBuffer->endGroup();
    mOfflineRosterBuffer->sync();

    // Update account's avoid list, in case they get added back
    accountWrapper->setContactsToAvoid(currentList);

    delete op;
}

bool CDTpController::registerDBusObject()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        warning() << "Could not connect to DBus:" << connection.lastError();
        return false;
    }

    if (!connection.registerObject(DBusObjectPath, this)) {
        warning() << "Could not register DBus object '/':" <<
            connection.lastError();
        return false;
    }
    return true;
}

PendingOfflineRemoval::PendingOfflineRemoval(CDTpAccountPtr accountWrapper,
        const QStringList &contacts, QObject *parent) : QObject(parent)
{
    mContactIds = contacts;
    mAccountWrapper = accountWrapper;

    onRosterChanged(mAccountWrapper);
    connect(mAccountWrapper.data(),
            SIGNAL(rosterChanged(CDTpAccountPtr)),
            SLOT(onRosterChanged(CDTpAccountPtr)));
}

PendingOfflineRemoval::~PendingOfflineRemoval()
{
}

void PendingOfflineRemoval::onRosterChanged(CDTpAccountPtr accountWrapper)
{
    if (!accountWrapper->hasRoster()) {
        return;
    }

    debug() << "Account has a roster, start removing contacts from server";

    mManager = accountWrapper->account()->connection()->contactManager();
    QList<Tp::ContactPtr> contactsToRemove;
    Q_FOREACH(const QString contactId, mContactIds) {
        Q_FOREACH(const Tp::ContactPtr tpcontact, mManager->allKnownContacts()) {
            if (tpcontact->id() == contactId) {
                contactsToRemove << tpcontact;
            }
        }
    }

    Tp::PendingOperation *call = mManager->removeContacts(contactsToRemove);
    connect(call,
            SIGNAL(finished(Tp::PendingOperation *)),
            SLOT(onContactsRemoved(Tp::PendingOperation *)));
}

void PendingOfflineRemoval::onContactsRemoved(Tp::PendingOperation *op)
{
    // Failed to remove contacts, will retry next time account goes online
    if (op->isError()) {
        return;
    }

    Q_EMIT finished(this);
}
