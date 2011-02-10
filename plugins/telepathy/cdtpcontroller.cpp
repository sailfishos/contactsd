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

#include "cdtpcontroller.h"

#include "cdtpstorage.h"

#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountPropertyFilter>
#include <TelepathyQt4/AndFilter>
#include <TelepathyQt4/NotFilter>
#include <TelepathyQt4/AccountSet>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>

CDTpController::CDTpController(QObject *parent) : QObject(parent)
{
    qDebug() << "Creating storage";
    mStorage = new CDTpStorage(this);
    connect(mStorage,
            SIGNAL(syncStarted(CDTpAccountPtr)),
            SLOT(onSyncStarted(CDTpAccountPtr)));
    connect(mStorage,
            SIGNAL(syncEnded(CDTpAccountPtr, int, int)),
            SLOT(onSyncEnded(CDTpAccountPtr, int, int)));

    qDebug() << "Creating account manager";
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
}

CDTpController::~CDTpController()
{
}

void CDTpController::onAccountManagerReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qDebug() << "Could not make account manager ready:" <<
            op->errorName() << "-" << op->errorMessage();
        return;
    }

    qDebug() << "Account manager ready";

    Tp::AccountPropertyFilterPtr filter1 = Tp::AccountPropertyFilter::create();
    filter1->addProperty("valid", true);
    filter1->addProperty("enabled", true);
    filter1->addProperty("hasBeenOnline", true);

    Tp::AccountPropertyFilterPtr normalizedNameFilter = Tp::AccountPropertyFilter::create();
    normalizedNameFilter->addProperty("normalizedName", QString());
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
        insertAccount(account);
    }

    mStorage->syncAccounts(mAccounts.values());
}

void CDTpController::onAccountAdded(const Tp::AccountPtr &account)
{
    if (mAccounts.contains(account)) {
        qWarning() << "Internal error, account was already in controller";
        return;
    }

    CDTpAccountPtr accountWrapper = insertAccount(account);
    mStorage->syncAccount(accountWrapper);
}

void CDTpController::onAccountRemoved(const Tp::AccountPtr &account)
{
    if (!mAccounts.contains(account)) {
        qWarning() << "Internal error, account was not in controller";
        return;
    }
    mStorage->removeAccount(mAccounts[account]);
    mAccounts.remove(account);
}

CDTpAccountPtr CDTpController::insertAccount(const Tp::AccountPtr &account)
{
    qDebug() << "Creating wrapper for account" << account->objectPath();
    CDTpAccountPtr accountWrapper = CDTpAccountPtr(new CDTpAccount(account, this));
    mAccounts.insert(account, accountWrapper);

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
    Q_EMIT importStarted(accountWrapper->providerName(), account->objectPath());
}

void CDTpController::onSyncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved)
{
    Tp::AccountPtr account = accountWrapper->account();
    Q_EMIT importEnded(accountWrapper->providerName(), account->objectPath(),
        contactsAdded, contactsRemoved, 0);
}
