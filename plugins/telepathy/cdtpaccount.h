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
        All         = (1 << 5) -1
    };
    Q_DECLARE_FLAGS(Changes, Change)

    CDTpAccount(const Tp::AccountPtr &account, QObject *parent = 0);
    ~CDTpAccount();

    Tp::AccountPtr account() const { return mAccount; }
    QString providerName() const;
    QList<CDTpContactPtr> contacts() const;
    CDTpContactPtr contact(const QString &id) const;
    bool hasRoster() const { return mHasRoster; };
    void firstTimeSeen();

Q_SIGNALS:
    void changed(CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes);
    void rosterChanged(CDTpAccountPtr accountWrapper);
    void rosterUpdated(CDTpAccountPtr acconutWrapper,
            const QList<CDTpContactPtr> &contactsAdded,
            const QList<CDTpContactPtr> &contactsRemoved);
    void rosterContactChanged(CDTpContactPtr contactWrapper, CDTpContact::Changes changes);

private Q_SLOTS:
    void onAccountDisplayNameChanged();
    void onAccountNicknameChanged();
    void onAccountCurrentPresenceChanged();
    void onAccountAvatarChanged();
    void onAccountConnectionChanged(const Tp::ConnectionPtr &connection);
    void onAccountContactChanged(CDTpContactPtr contactWrapper,
            CDTpContact::Changes changes);
    void onAccountStateChanged(bool enabled);
    void onContactListStateChanged(Tp::ContactListState);
    void onRosterChangedTimeout();
    void onAllKnownContactsChanged(const Tp::Contacts &contactsAdded,
            const Tp::Contacts &contactsRemoved);

private:
    void setConnection(const Tp::ConnectionPtr &connection);
    void setContactManager(const Tp::ContactManagerPtr &contactManager);
    CDTpContactPtr insertContact(const Tp::ContactPtr &contact);
    void maybeRequestExtraInfo(Tp::ContactPtr contact);
    void emitRosterChanged();

private:
    Tp::AccountPtr mAccount;
    QHash<QString, CDTpContactPtr> mContacts;
    QTimer mRosterChangedTimer;
    bool mHasRoster;
    bool mFirstSeen;
    bool mBlockSignals;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpAccount::Changes)

#endif // CDTPACCOUNT_H
