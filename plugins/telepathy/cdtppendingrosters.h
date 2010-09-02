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

#ifndef CDTPPENDINGROSTERS_H
#define CDTPPENDINGROSTERS_H

#include "cdtpcontact.h"

#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/ConnectionManager>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingAccount>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Types>

#include <QList>

class TelepathyController;

class PendingRosters : public QObject
{
    Q_OBJECT
public:
    explicit PendingRosters(QObject *parent = 0);
    Tp::Contacts rosterList() const;
    QList<QSharedPointer<TpContact> > telepathyRosterList() const;
    QList<Tp::ConnectionPtr> contactConnections();
    QList<QSharedPointer<TpContact> > newRosters() const;
    bool isError() const;
    QString errorName() const;
    QString errorMessage() const;
Q_SIGNALS:
    void contact(QSharedPointer<TpContact>&);
    void contactsAdded(QList<QSharedPointer<TpContact> >);
    void contactsRemoved(QList<QSharedPointer<TpContact> >);
    void finished (PendingRosters *operation);
private Q_SLOTS:
    void onConnectionReady(Tp::PendingOperation * po);
    void onAccountReady(Tp::PendingOperation* op);
    void onHaveConnectionChanged(bool);
    void onConnectionStatusChanged(Tp::ConnectionStatus status, Tp::ConnectionStatusReason reason);
    void onAllKnownContactsChanged(const Tp::Contacts& added, const Tp::Contacts& removed) ;
private:
    Q_DISABLE_COPY(PendingRosters)
    friend class TelepathyController;
    void addRequestForAccount(Tp::AccountPtr);
    void setFinished();
    void setFinishedWithError(const QString&, const QString&);

    Tp::Contacts mContacts;
    QList<QSharedPointer<TpContact> >  mTpContacts;
    QList<Tp::ConnectionPtr> mConnectionList;
    Tp::AccountPtr mAccount;
    int mCapCount;
    bool mWasOnline;
    bool mHasError;
    QString mErrorName;
    QString mErrorMessage;

};

#endif // CDTPPENDINGROSTERS_H
