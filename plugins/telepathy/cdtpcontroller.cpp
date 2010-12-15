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
      mImportActive(false)
{
    qDebug() << "Creating storage";
    mStorage = new CDTpStorage(this);

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
    return mImportActive;
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

void CDTpController::onAccountRosterFetching(CDTpAccount *accountWrapper)
{
    Tp::AccountPtr account = accountWrapper->account();

    setImportStarted(account);
}

void CDTpController::onAccountRosterChanged(CDTpAccount *accountWrapper,
        bool haveRoster)
{
    Tp::AccountPtr account = accountWrapper->account();

    if (haveRoster) {
        // TODO: emit importEnded once syncAccountContacts return the
        //       number of contacts actually added
        mStorage->syncAccountContacts(accountWrapper);
        setImportEnded(account, accountWrapper->contacts().size(), 0);
    } else {
        mStorage->setAccountContactsOffline(accountWrapper);
    }
}

void CDTpController::onAccountRosterUpdated(CDTpAccount *accountWrapper,
        const QList<CDTpContactPtr> &contactsAdded,
        const QList<CDTpContactPtr> &contactsRemoved)
{
    Tp::AccountPtr account = accountWrapper->account();

    mStorage->syncAccountContacts(accountWrapper, contactsAdded,
            contactsRemoved);
    setImportEnded(account, contactsAdded.size(), contactsRemoved.size());
}

void CDTpController::insertAccount(const Tp::AccountPtr &account)
{
    qDebug() << "Creating wrapper for account" << account->objectPath();
    CDTpAccount *accountWrapper = new CDTpAccount(account, this);
    connect(accountWrapper,
            SIGNAL(ready(CDTpAccount *)),
            mStorage,
            SLOT(syncAccount(CDTpAccount *)));
    connect(accountWrapper,
            SIGNAL(changed(CDTpAccount *, CDTpAccount::Changes)),
            mStorage,
            SLOT(syncAccount(CDTpAccount *, CDTpAccount::Changes)));
    connect(accountWrapper,
            SIGNAL(rosterFetching(CDTpAccount*)),
            SLOT(onAccountRosterFetching(CDTpAccount *)));
    connect(accountWrapper,
            SIGNAL(rosterChanged(CDTpAccount *, bool)),
            SLOT(onAccountRosterChanged(CDTpAccount *, bool)));
    connect(accountWrapper,
            SIGNAL(rosterUpdated(CDTpAccount *,
                    const QList<CDTpContactPtr> &,
                    const QList<CDTpContactPtr> &)),
            SLOT(onAccountRosterUpdated(CDTpAccount *,
                    const QList<CDTpContactPtr> &,
                    const QList<CDTpContactPtr> &)));
    connect(accountWrapper,
            SIGNAL(rosterContactChanged(CDTpAccount *,
                    CDTpContactPtr, CDTpContact::Changes)),
            mStorage,
            SLOT(syncAccountContact(CDTpAccount *,
                    CDTpContactPtr, CDTpContact::Changes)));

    mAccounts.insert(account->objectPath(), accountWrapper);
}

void CDTpController::removeAccount(const QString &accountObjectPath)
{
    mStorage->removeAccount(accountObjectPath);
    delete mAccounts.take(accountObjectPath);
}

void CDTpController::setImportStarted(const Tp::AccountPtr &account)
{
    qDebug() << Q_FUNC_INFO;

    mImportActive = true;
    Q_EMIT importStarted(account->serviceName(), account->objectPath());
}

void CDTpController::setImportEnded(const Tp::AccountPtr &account, int contactsAdded, int contactsRemoved)
{
    if (mImportActive) {
        qDebug() << Q_FUNC_INFO;
        mImportActive = false;
        Q_EMIT importEnded(account->serviceName(), account->objectPath(),
                           contactsAdded, contactsRemoved, 0);
    }
}
