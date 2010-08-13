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

#include "telepathycontroller.h"
#include <QtDebug>
#include <QDir>
#include <QQueue>
//telepathy includes
#include <TelepathyQt4/Types>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/PendingContacts>


class TelepathyController::Private
{

public:
    Private() :
            error(false) {}

    ~Private() {
    }

    Tp::AccountManagerPtr accountManager;
    bool error;
};


TelepathyController::TelepathyController(QObject * parent, bool cache): QObject(parent), d(new Private)
{
    Q_UNUSED(cache);
    qDebug() << Q_FUNC_INFO;
    Tp::registerTypes();
    d->accountManager = Tp::AccountManager::create();
    d->error = true;
}

TelepathyController::~TelepathyController()
{
    delete d;
}

void TelepathyController::requestIMAccounts()
{
   if (d->accountManager->isReady()) {
       emit finished();
       return;
   }

   connect(d->accountManager->becomeReady(), SIGNAL(finished(Tp::PendingOperation*)),
           this, SLOT(onAmFinished(Tp::PendingOperation*)));
}

QList<Tp::AccountPtr> TelepathyController::getIMAccount(const QString& protocol)
{
    QList<Tp::AccountPtr> rv;

    foreach (Tp::AccountPtr account, d->accountManager->validAccounts() ) {
        if (account->protocol() == protocol) {
            rv.append(account);
        }
    }

    return rv;
}

void TelepathyController::onAmFinished(Tp::PendingOperation* op)
{
    if (op->isError()) {
        qDebug() << Q_FUNC_INFO << op->errorName() << op->errorMessage() ;
        d->error = true;
        qWarning() << Q_FUNC_INFO << ": Account manager error: " << op->errorName() << ": " << op->errorMessage();
    }

    emit finished();
}
PendingRosters * TelepathyController::requestRosters(Tp::AccountPtr account)
{
    qDebug() << Q_FUNC_INFO << ": Requesting roster for account: " << account->objectPath();
    PendingRosters * roster = new PendingRosters(this);
    roster->addRequestForAccount(account);
    return roster;
}

bool TelepathyController::isError() const
{
    if (!d->accountManager->isReady()) {
        return true;
    }

    if (d->error) {
        return true;
    }

    return false;
}
