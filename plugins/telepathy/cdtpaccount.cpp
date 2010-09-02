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

#include "cdtpaccount.h"

#include <TelepathyQt4/Account>
#include <TelepathyQt4/PendingReady>

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/QLive>
#include <QtTracker/Tracker>

#include <QImage>

using namespace SopranoLive;


class CDTpAccount::Private
{
    public:
        Private() {}
        ~Private() {}
        Tp::AccountPtr mAccount;
};

QDebug operator<<(QDebug dbg, const Tp::Account &account)
{
    dbg.nospace() << "Tp::Account("
            << "displayName:"  << account.displayName()
            << " nickName:"    << account.nickname()
            //<< " parameters:"  << account.parameters()
            << " uniqueID:"    << account.uniqueIdentifier()
            << ')';
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const Tp::AccountPtr &accountPtr)
{
    return dbg.nospace() << *(accountPtr.constData());
}

QDebug operator<<(QDebug dbg, const CDTpAccount &account)
{
    return dbg.nospace() << account.account();
}

CDTpAccount::CDTpAccount(Tp::AccountPtr account, QObject* parent) : QObject(parent), d(new Private)
{
     d->mAccount = account;
     connect(d->mAccount->becomeReady(Tp::Account::FeatureCore | Tp::Account::FeatureAvatar), SIGNAL(finished(Tp::PendingOperation*)),
             this, SLOT(onAccountReady(Tp::PendingOperation*)));
}


CDTpAccount::~CDTpAccount()
{
    delete d;
}

void CDTpAccount::onAccountReady(Tp::PendingOperation* op)
{
    if (op->isError()) {
        return;
    }

    qDebug() << Q_FUNC_INFO << ": Account became ready: " << d->mAccount->objectPath();

    connect(d->mAccount.data(), SIGNAL(displayNameChanged (const QString &)),
            this,
            SLOT(onDisplayNameChanged(const QString&)));
    connect(d->mAccount.data(), SIGNAL(iconChanged (const QString &)),
                this, SLOT(onIconChanged(const QString&)));
    connect(d->mAccount.data(), SIGNAL (nicknameChanged (const QString &)),
            this, SLOT(onNicknameChanged(const QString&)));
    connect(d->mAccount.data(), SIGNAL(avatarChanged (const Tp::Avatar &)),
            this, SLOT(onAvatarUpdated(const Tp::Avatar&)));
    connect(d->mAccount.data(), SIGNAL(currentPresenceChanged(const Tp::SimplePresence&)),
            this, SLOT(onCurrentPresenceChanged(const Tp::SimplePresence&)));

}

Tp::AccountPtr CDTpAccount::account() const
{
    return d->mAccount;
}

void CDTpAccount::onCurrentPresenceChanged(const Tp::SimplePresence& presence)
{
     Q_UNUSED(presence);
     Q_EMIT accountChanged(this, Presence);
}
void CDTpAccount::onDisplayNameChanged(const QString& name)
{
    Q_UNUSED(name);
    Q_EMIT  accountChanged(this, Alias);
}

void CDTpAccount::onIconChanged(const QString& icon)
{
    Q_UNUSED(icon);
    Q_EMIT accountChanged(this, Icon);
}
void CDTpAccount::onNicknameChanged(const QString& nick)
{
    Q_UNUSED(nick);
    Q_EMIT accountChanged(this, NickName);
}

void CDTpAccount::onAvatarUpdated(const Tp::Avatar& avatar)
{
    Q_UNUSED(avatar);
    Q_EMIT accountChanged(this, Avatar);
}
