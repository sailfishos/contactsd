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

#ifndef CDTPCONTROLLER_H
#define CDTPCONTROLLER_H

#include "cdtpaccount.h"
#include "cdtpcontact.h"
#include "cdtpstorage.h"

#include <TelepathyQt4/Types>

#include <QList>
#include <QObject>
#include <QSettings>

class PendingOfflineRemoval;

class CDTpController : public QObject
{
    Q_OBJECT

public:
    CDTpController(QObject *parent = 0);
    ~CDTpController();

Q_SIGNALS:
    void importStarted(const QString &service, const QString &account);
    void importEnded(const QString &service, const QString &account, int contactsAdded, int contactsRemoved, int contactsMerged);

public Q_SLOTS:
    void inviteBuddies(const QString &accountPath, const QStringList &imIds);
    void removeBuddies(const QString &accountPath, const QStringList &imIds);
    void onRosterChanged(CDTpAccountPtr accountWrapper);

private Q_SLOTS:
    void onAccountManagerReady(Tp::PendingOperation *op);
    void onAccountAdded(const Tp::AccountPtr &account);
    void onAccountRemoved(const Tp::AccountPtr &account);
    void onSyncStarted(CDTpAccountPtr accountWrapper);
    void onSyncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved);
    void onInvitationFinished(Tp::PendingOperation *op);
    void onRemovalFinished(Tp::PendingOperation *op);

private:
    CDTpAccountPtr insertAccount(const Tp::AccountPtr &account, bool newAccount);
    void removeAccount(const QString &accountObjectPath);
    void maybeStartOfflineOperations(CDTpAccountPtr accountWrapper);
    QStringList updateOfflineRosterBuffer(const QString group, const QString accountPath,
        const QStringList idsToAdd, const QStringList idsToRemove);
    bool registerDBusObject();

private:
    CDTpStorage *mStorage;
    Tp::AccountManagerPtr mAM;
    Tp::AccountSetPtr mAccountSet;
    QHash<QString, CDTpAccountPtr> mAccounts;
    QSettings *mOfflineRosterBuffer;
};

class CDTpRemovalOperation : public Tp::PendingOperation
{
    Q_OBJECT

public:
    CDTpRemovalOperation(CDTpAccountPtr accountWrapper, const QStringList &contactIds);
    QStringList contactIds() const { return mContactIds; }
    CDTpAccountPtr accountWrapper() const { return mAccountWrapper; }

private Q_SLOTS:
    void onContactsRemoved(Tp::PendingOperation *op);

private:
    QStringList mContactIds;
    CDTpAccountPtr mAccountWrapper;
};

class CDTpInvitationOperation : public Tp::PendingOperation
{
    Q_OBJECT

public:
    CDTpInvitationOperation(CDTpAccountPtr accountWrapper, const QStringList &contactIds);
    QStringList contactIds() const { return mContactIds; }
    CDTpAccountPtr accountWrapper() const { return mAccountWrapper; }

private Q_SLOTS:
    void onContactsRetrieved(Tp::PendingOperation *op);
    void onPresenceSubscriptionRequested(Tp::PendingOperation *op);

private:
    QStringList mContactIds;
    CDTpAccountPtr mAccountWrapper;
};

#endif // CDTPCONTROLLER_H
