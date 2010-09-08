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

#ifndef CDTPACCOUNT_H
#define CDTPACCOUNT_H

#include <TelepathyQt4/Account>
#include <TelepathyQt4/Constants>
#include <TelepathyQt4/Contact>
#include <TelepathyQt4/Types>

#include <QObject>

namespace Tp
{
    class PendingOperation;
}

class CDTpContact;

class CDTpAccount : public QObject
{
    Q_OBJECT

public:
    enum Change {
        DisplayName = 0x1,
        Nickname    = 0x2,
        Presence    = 0x4,
        Avatar      = 0x8,
        All         = 0xFFFF
    };
    Q_DECLARE_FLAGS(Changes, Change)

    CDTpAccount(const Tp::AccountPtr &account, QObject *parent = 0);
    ~CDTpAccount();

    Tp::AccountPtr account() const { return mAccount; }

    QList<CDTpContact *> contacts() const;

Q_SIGNALS:
    void ready(CDTpAccount *account);
    void changed(CDTpAccount *account, CDTpAccount::Changes changes);
    void rosterChanged(CDTpAccount *account, bool haveRoster);
    void rosterUpdated(CDTpAccount *acconut,
            const QList<CDTpContact *> &contactsAdded,
            const QList<CDTpContact *> &contactsRemoved);

private Q_SLOTS:
    void onAccountReady(Tp::PendingOperation *op);
    void onAccountDisplayNameChanged();
    void onAccountNicknameChanged();
    void onAccountCurrentPresenceChanged();
    void onAccountAvatarChanged();
    void onAccountHaveConnectionChanged(bool haveConnection);
    void onAccountConnectionStatusChanged(Tp::Connection::Status status);
    void onAccountConnectionReady(Tp::PendingOperation *op);
    void onAccountConnectionRosterReady(Tp::PendingOperation *op);
    void onAccountContactsUpgraded(Tp::PendingOperation *op);
    void onAccountContactsChanged(const Tp::Contacts &contactsAdded,
            const Tp::Contacts &contactsRemoved);

private:
    void introspectAccountConnection();
    void introspectAccountConnectionRoster();
    void upgradeContacts(const Tp::Contacts &contacts);
    CDTpContact *insertContact(const Tp::ContactPtr &contact);
    void clearContacts();

    Tp::AccountPtr mAccount;
    bool mRosterReady;
    QHash<Tp::ContactPtr, CDTpContact *> mContacts;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpAccount::Changes)

#endif // CDTPACCOUNT_H
