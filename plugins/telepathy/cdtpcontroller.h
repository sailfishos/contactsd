/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

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
    void error(int code, const QString &message);

public Q_SLOTS:
    void inviteBuddies(const QString &accountPath, const QStringList &imIds);
    void inviteBuddiesOnContact(const QString &accountPath, const QStringList &imIds, uint localId);
    void removeBuddies(const QString &accountPath, const QStringList &imIds);
    void onRosterChanged(CDTpAccountPtr accountWrapper);

private Q_SLOTS:
    void onAccountManagerReady(Tp::PendingOperation *op);
    void onAccountAdded(const Tp::AccountPtr &account);
    void onAccountRemoved(const Tp::AccountPtr &account);
    void onSyncStarted(Tp::AccountPtr account);
    void onSyncEnded(Tp::AccountPtr account, int contactsAdded, int contactsRemoved);
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
    CDTpInvitationOperation(CDTpStorage *storage,
                            CDTpAccountPtr accountWrapper,
                            const QStringList &contactIds,
                            uint contactLocalId);
    QStringList contactIds() const { return mContactIds; }
    CDTpAccountPtr accountWrapper() const { return mAccountWrapper; }

private Q_SLOTS:
    void onContactsRetrieved(Tp::PendingOperation *op);
    void onPresenceSubscriptionRequested(Tp::PendingOperation *op);

private:
    CDTpStorage *mStorage;
    QStringList mContactIds;
    CDTpAccountPtr mAccountWrapper;
    uint mContactLocalId;
};

#endif // CDTPCONTROLLER_H
