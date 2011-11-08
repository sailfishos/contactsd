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

#ifndef CDTPACCOUNT_H
#define CDTPACCOUNT_H

#include <QObject>

#include <TelepathyQt4/Account>
#include <TelepathyQt4/Constants>
#include <TelepathyQt4/Contact>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/PendingOperation>

#include "types.h"
#include "cdtpcontact.h"

class CDTpAccount : public QObject, public Tp::RefCounted
{
    Q_OBJECT

public:
    enum Change {
        DisplayName = (1 << 0),
        Nickname    = (1 << 1),
        Presence    = (1 << 2),
        Avatar      = (1 << 3),
        Enabled     = (1 << 4),
        All         = (1 << 5) -1
    };
    Q_DECLARE_FLAGS(Changes, Change)

    CDTpAccount(const Tp::AccountPtr &account,
            const QStringList &contactsToAvoid = QStringList(),
            bool newAccount = false, QObject *parent = 0);
    ~CDTpAccount();

    Tp::AccountPtr account() const { return mAccount; }
    QList<CDTpContactPtr> contacts() const;
    QHash<QString, CDTpContact::Changes> rosterChanges() const;
    CDTpContactPtr contact(const QString &id) const;
    bool hasRoster() const { return mHasRoster; };
    bool isNewAccount() const { return mNewAccount; };
    bool isEnabled() const { return mAccount->isEnabled(); };
    QStringList contactsToAvoid() const { return mContactsToAvoid; }
    void setContactsToAvoid(const QStringList &contactIds);

    void emitSyncEnded(int contactsAdded, int contactsRemoved);
    QHash<QString, CDTpContact::Info> rosterCache() const;
    void setRosterCache(const QHash<QString, CDTpContact::Info> &rosterCache);

Q_SIGNALS:
    void changed(CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes);
    void rosterChanged(CDTpAccountPtr accountWrapper);
    void rosterUpdated(CDTpAccountPtr acconutWrapper,
            const QList<CDTpContactPtr> &contactsAdded,
            const QList<CDTpContactPtr> &contactsRemoved);
    void rosterContactChanged(CDTpContactPtr contactWrapper, CDTpContact::Changes changes);
    void syncStarted(Tp::AccountPtr account);
    void syncEnded(Tp::AccountPtr account, int contactsAdded, int contactsRemoved);

private Q_SLOTS:
    void onAccountDisplayNameChanged();
    void onAccountNicknameChanged();
    void onAccountCurrentPresenceChanged();
    void onAccountAvatarChanged();
    void onAccountStateChanged();
    void onAccountConnectionChanged(const Tp::ConnectionPtr &connection);
    void onContactListStateChanged(Tp::ContactListState);
    void onAccountContactChanged(CDTpContactPtr contactWrapper,
            CDTpContact::Changes changes);
    void onAllKnownContactsChanged(const Tp::Contacts &contactsAdded,
            const Tp::Contacts &contactsRemoved);
    void onDisconnectTimeout();

private:
    void setConnection(const Tp::ConnectionPtr &connection);
    void setContactManager(const Tp::ContactManagerPtr &contactManager);
    CDTpContactPtr insertContact(const Tp::ContactPtr &contact);
    void maybeRequestExtraInfo(Tp::ContactPtr contact);
    void makeRosterCache();

private:
    Tp::AccountPtr mAccount;
    Tp::ConnectionPtr mCurrentConnection;
    QHash<QString, CDTpContactPtr> mContacts;
    QHash<QString, CDTpContact::Info> mRosterCache;
    QStringList mContactsToAvoid;
    QTimer mDisconnectTimeout;
    bool mHasRoster;
    bool mNewAccount;
    bool mImporting;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpAccount::Changes)

#endif // CDTPACCOUNT_H
