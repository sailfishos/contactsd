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

#include "cdtpcontroller.h"

#include "cdtppendingrosters.h"

#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Types>

#include <QDir>
#include <QQueue>
#include <QtDebug>

class CDTpController::Private
{
public:
    Private() : error(false) {}

    Tp::AccountManagerPtr accountManager;
    bool error;
};

CDTpController::CDTpController(QObject *parent, bool cache)
    : QObject(parent),
      d(new Private)
{
    Q_UNUSED(cache);
    qDebug() << Q_FUNC_INFO;
    Tp::registerTypes();
    d->accountManager = Tp::AccountManager::create();
    d->error = true;
}

CDTpController::~CDTpController()
{
    delete d;
}

void CDTpController::requestIMAccounts()
{
    if (d->accountManager->isReady()) {
        emit finished();
        return;
    }

    connect(d->accountManager->becomeReady(),
            SIGNAL(finished(Tp::PendingOperation *)),
            this,
            SLOT(onAmFinished(Tp::PendingOperation *)));
}

QList<Tp::AccountPtr> CDTpController::getIMAccount(const QString &protocol)
{
    QList<Tp::AccountPtr> rv;

    foreach(Tp::AccountPtr account, d->accountManager->validAccounts()) {
        if (account->protocol() == protocol) {
            rv.append(account);
        }
    }

    return rv;
}

void CDTpController::onAmFinished(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qDebug() << Q_FUNC_INFO << op->errorName() << op->errorMessage() ;
        d->error = true;
        qWarning() << Q_FUNC_INFO << ": Account manager error: " <<
            op->errorName() << ": " << op->errorMessage();
    }

    emit finished();
}

CDTpPendingRosters *CDTpController::requestRosters(Tp::AccountPtr account)
{
    qDebug() << Q_FUNC_INFO << ": Requesting roster for account: " <<
        account->objectPath();
    CDTpPendingRosters *roster = new CDTpPendingRosters(this);
    roster->addRequestForAccount(account);
    return roster;
}

bool CDTpController::isError() const
{
    if (!d->accountManager->isReady() || d->error) {
        return true;
    }

    return false;
}
