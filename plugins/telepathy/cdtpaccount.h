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
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingOperation>

#include <QDebug>
#include <QObject>

/**
 * Wraps Tp::Account to monitor signals
**/

class CDTpAccount : public QObject
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

    explicit CDTpAccount(Tp::AccountPtr, QObject* parent = 0 );
    virtual ~CDTpAccount();
    Tp::AccountPtr account() const;

Q_SIGNALS:
    void accountChanged(CDTpAccount*, CDTpAccount::Changes);

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
QDebug operator<<(QDebug dbg, const CDTpAccount &account);

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpAccount::Changes)

#endif // CDTPACCOUNT_H
