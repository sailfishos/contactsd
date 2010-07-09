/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#ifndef TELEPATHY_ACCOUNT_H
#define TELEPATHY_ACCOUNT_H

#include <QtCore/QDebug>
#include <QtCore/QObject>

#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/ContactManager>
#include "telepathycontact.h"
#include "pendingrosters.h"

/**
 * Wraps Tp::Account to monitor signals
**/

class TelepathyAccount : public QObject
{
    Q_OBJECT
public:
    enum Change { Alias     = 0x1,
                  NickName  = 0x2,
                  Presence  = 0x4,
                  Status    = 0x8,
                  Icon      = 0x10,
                  Avatar    = 0x20,
                  Parameter = 0x40,
                  All       = 0x7F
                          };

    Q_DECLARE_FLAGS(Changes, Change)

    explicit TelepathyAccount(Tp::AccountPtr, QObject* parent = 0 );
    virtual ~TelepathyAccount();
    Tp::AccountPtr account() const;

Q_SIGNALS:
    void accountChanged(TelepathyAccount*, TelepathyAccount::Changes);

private Q_SLOTS:
    void onAccountReady(Tp::PendingOperation*);
    void onDisplayNameChanged(const QString&);
    void onIconChanged(const QString&);
    void onNicknameChanged(const QString&);
    void onAvatarUpdated(const Tp::Avatar&);
    void onCurrentPresenceChanged(const Tp::SimplePresence& presence);

private:
    class Private;
    Private * const d;
};

QDebug operator<<(QDebug dbg, const Tp::Account &account);
QDebug operator<<(QDebug dbg, const Tp::AccountPtr &accountPtr);
QDebug operator<<(QDebug dbg, const TelepathyAccount &account);

Q_DECLARE_OPERATORS_FOR_FLAGS(TelepathyAccount::Changes)

#endif // TELEPATHY_ACCOUNT_H
