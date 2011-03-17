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
    void inviteBuddy(const QString &accountPath, const QString &imId);
    void removeBuddy(const QString &accountPath, const QString &imId);

private Q_SLOTS:
    void onAccountManagerReady(Tp::PendingOperation *op);
    void onAccountAdded(const Tp::AccountPtr &account);
    void onAccountRemoved(const Tp::AccountPtr &account);
    void onAccountReady(CDTpAccountPtr accountWrapper);
    void onAccountOnlinenessChanged(bool online);
    void onSyncStarted(CDTpAccountPtr accountWrapper);
    void onSyncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved);
    void onContactsRemoved(Tp::PendingOperation *op);
    void onInviteContactRetrieved(Tp::PendingOperation *op);
    void onPresenceSubscriptionRequested(Tp::PendingOperation *op);
    void clearOfflineBuffer(PendingOfflineRemoval *pr);

private:
    CDTpAccountPtr insertAccount(const Tp::AccountPtr &account, bool newAccount);
    void removeAccount(const QString &accountObjectPath);

    void setImportStarted(const Tp::AccountPtr &account);
    void setImportEnded(const Tp::AccountPtr &account,
                        int contactsAdded, int contactsRemoved);
    bool registerDBusObject();
    void checkOfflineOperations();

private:
    CDTpStorage *mStorage;
    Tp::AccountManagerPtr mAM;
    Tp::AccountSetPtr mAccountSet;
    QHash<QString, CDTpAccountPtr> mAccounts;
    QSettings *mOfflineRosterBuffer;
};

class PendingOfflineRemoval : public QObject
{
    Q_OBJECT

public:
    PendingOfflineRemoval(const QString &accountPath, const QStringList &contactids,
            QObject *parent = 0);
    virtual ~PendingOfflineRemoval();
    QString accountPath() const { return mAccountPath;}
    bool isError() const {return mIsError;}
    QString errorName() const { return mErrorName;}
    QString errorMessage() const { return mErrorMessage;}
Q_SIGNALS:
    void finished(PendingOfflineRemoval *op);

private Q_SLOTS:
    void onOfflineAccountReady(Tp::PendingOperation *po);
    void onConnectionReady(Tp::PendingOperation * op);
    void onContactsRemoved(Tp::PendingOperation *op);
    void onConnectionChanged(Tp::ConnectionPtr connection);
private:
    QStringList mContactIds;
    QString mAccountPath;
    Tp::AccountPtr mAccount;
    bool mIsError;
    QString mErrorName;
    QString mErrorMessage;
};
#endif // CDTPCONTROLLER_H
