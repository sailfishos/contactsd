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

#include "cdtpcontact.h"

#include <TelepathyQt4/Account>
#include <TelepathyQt4/Constants>
#include <TelepathyQt4/Contact>
#include <TelepathyQt4/Types>

#include <QObject>

namespace Tp
{
    class PendingOperation;
}

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
    void ready(CDTpAccount *accountWrapper);
    void changed(CDTpAccount *accountWrapper, CDTpAccount::Changes changes);
    void rosterChanged(CDTpAccount *accountWrapper, bool haveRoster);
    void rosterUpdated(CDTpAccount *acconutWrapper,
            const QList<CDTpContact *> &contactsAdded,
            const QList<CDTpContact *> &contactsRemoved);
    void rosterContactChanged(CDTpAccount *accountWrapper,
            CDTpContact *contactWrapper, CDTpContact::Changes changes);

private Q_SLOTS:
    void onAccountReady(Tp::PendingOperation *op);
    void onAccountNormalizedNameChanged();
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
    void onAccountContactChanged(CDTpContact *contactWrapper,
            CDTpContact::Changes changes);

private:
    void introspectAccountConnection();
    void introspectAccountConnectionRoster();
    void upgradeContacts(const Tp::Contacts &contacts);
    CDTpContact *insertContact(const Tp::ContactPtr &contact);
    void clearContacts();

    Tp::AccountPtr mAccount;
    bool mIntrospectingRoster;
    bool mRosterReady;
    QHash<Tp::ContactPtr, CDTpContact *> mContacts;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpAccount::Changes)

#endif // CDTPACCOUNT_H
