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
#include <TelepathyQt4/AccountSet>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>

CDTpController::CDTpController(QObject *parent)
    : QObject(parent),
      mImportActive(0)
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
    mAM = Tp::AccountManager::create();

    qDebug() << "Introspecting account manager";
    connect(mAM->becomeReady(),
            SIGNAL(finished(Tp::PendingOperation*)),
            SLOT(onAccountManagerReady(Tp::PendingOperation*)));
}

CDTpController::~CDTpController()
{
}

bool CDTpController::hasActiveImports() const
{
    return mImportActive > 0;
}

void CDTpController::onAccountManagerReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qDebug() << "Could not make account manager ready:" <<
            op->errorName() << "-" << op->errorMessage();
        return;
    }

    qDebug() << "Account manager ready";

    QVariantMap filter;
    filter.insert("valid", true);
    filter.insert("enabled", true);
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
    mStorage->syncAccountSet(mAccounts.keys());
}

void CDTpController::onAccountAdded(const Tp::AccountPtr &account)
{
    qDebug() << "Account" << account->objectPath() << "added";
    insertAccount(account);
}

void CDTpController::onAccountRemoved(const Tp::AccountPtr &account)
{
    removeAccount(account->objectPath());
}

void CDTpController::onAccountReady(CDTpAccountPtr accountWrapper)
{
    mStorage->syncAccount(accountWrapper);
}

void CDTpController::onAccountRosterChanged(CDTpAccountPtr accountWrapper,
        bool haveRoster)
{
    Tp::AccountPtr account = accountWrapper->account();

    if (haveRoster) {
        mStorage->syncAccountContacts(accountWrapper);
    } else {
        mStorage->setAccountContactsOffline(accountWrapper);
    }
}

void CDTpController::onAccountRosterUpdated(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contactsAdded,
        const QList<CDTpContactPtr> &contactsRemoved)
{
    Tp::AccountPtr account = accountWrapper->account();

    mStorage->syncAccountContacts(accountWrapper, contactsAdded,
            contactsRemoved);
}

void CDTpController::insertAccount(const Tp::AccountPtr &account)
{
    qDebug() << "Creating wrapper for account" << account->objectPath();
    CDTpAccountPtr accountWrapper = CDTpAccountPtr(new CDTpAccount(account, this));
    connect(accountWrapper.data(),
            SIGNAL(ready(CDTpAccountPtr)),
            SLOT(onAccountReady(CDTpAccountPtr)));
    connect(accountWrapper.data(),
            SIGNAL(changed(CDTpAccountPtr, CDTpAccount::Changes)),
            mStorage,
            SLOT(syncAccount(CDTpAccountPtr, CDTpAccount::Changes)));
    connect(accountWrapper.data(),
            SIGNAL(rosterChanged(CDTpAccountPtr , bool)),
            SLOT(onAccountRosterChanged(CDTpAccountPtr , bool)));
    connect(accountWrapper.data(),
            SIGNAL(rosterUpdated(CDTpAccountPtr ,
                    const QList<CDTpContactPtr> &,
                    const QList<CDTpContactPtr> &)),
            SLOT(onAccountRosterUpdated(CDTpAccountPtr ,
                    const QList<CDTpContactPtr> &,
                    const QList<CDTpContactPtr> &)));
    connect(accountWrapper.data(),
            SIGNAL(rosterContactChanged(CDTpAccountPtr ,
                    CDTpContactPtr, CDTpContact::Changes)),
            mStorage,
            SLOT(syncAccountContact(CDTpAccountPtr ,
                    CDTpContactPtr, CDTpContact::Changes)));

    mAccounts.insert(account->objectPath(), accountWrapper);
}

void CDTpController::removeAccount(const QString &accountObjectPath)
{
    mStorage->removeAccount(accountObjectPath);
    mAccounts.remove(accountObjectPath);
}

void CDTpController::onSyncStarted(CDTpAccountPtr accountWrapper)
{
    mImportActive++;

    Tp::AccountPtr account = accountWrapper->account();
    Q_EMIT importStarted(account->serviceName(), account->objectPath());
}

void CDTpController::onSyncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved)
{
    mImportActive--;

    Tp::AccountPtr account = accountWrapper->account();
    Q_EMIT importEnded(account->serviceName(), account->objectPath(),
        contactsAdded, contactsRemoved, 0);
}
