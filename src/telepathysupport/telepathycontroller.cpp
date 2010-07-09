/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */



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

//project includes

#include "telepathycontact.h"

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
