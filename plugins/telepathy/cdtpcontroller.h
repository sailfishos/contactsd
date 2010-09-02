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

#ifndef CDTPCONTROLLER_H
#define CDTPCONTROLLER_H

#include <TelepathyQt4/Account>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingOperation>

#include <QObject>

/**
 * Provides access to Telepathy Contacts
**/

class CDTpPendingRosters;

class CDTpController : public QObject
{
    Q_OBJECT
public:
    explicit CDTpController(QObject * parent = 0,  bool cache = false);
    virtual ~CDTpController();
    /*!
     *\brief Provides a set of pending roster contats, from all accounts
     *\returns Pointer to a Pending Roster
     */
    CDTpPendingRosters* requestRosters(Tp::AccountPtr account);

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

#endif // CDTPCONTROLLER_H
