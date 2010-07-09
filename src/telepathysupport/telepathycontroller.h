/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */


#ifndef TELEPATHY_CONTROLLER_H
#define TELEPATHY_CONTROLLER_H

#include <QtCore/QObject>

#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/ContactManager>
#include "telepathycontact.h"
#include "pendingrosters.h"

/**
 * Provides access to Telepathy Contacts
**/

class TelepathyController : public QObject
{
    Q_OBJECT
public:
    typedef QList<QSharedPointer<TelepathyContact> > ContactList;
    explicit TelepathyController(QObject * parent = 0,  bool cache = false);
    virtual ~TelepathyController();
    /*!
     *\brief Provides a set of pending roster contats, from all accounts
     *\returns Pointer to a Pending Roster
     */
    PendingRosters* requestRosters(Tp::AccountPtr account);

    QList<Tp::AccountPtr> getIMAccount(const QString& cmName);

    void requestIMAccounts();
    bool isError() const;
Q_SIGNALS:
    void finished();
public Q_SLOTS:
    void onAmFinished(Tp::PendingOperation* op);

private:
    class Private;
    Private * const d;
};

#endif // TELEPATHY_CONTROLLER_H
