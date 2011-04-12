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

#ifndef CDTPSTORAGE_H
#define CDTPSTORAGE_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include <QtSparql/QSparqlQuery>

#include "cdtpaccount.h"
#include "cdtpcontact.h"
#include "cdtpquery.h"

class CDTpStorage : public QObject
{
    Q_OBJECT

public:
    CDTpStorage(QObject *parent = 0);
    ~CDTpStorage();

Q_SIGNALS:
    void syncStarted(CDTpAccountPtr accountWrapper);
    void syncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved);

public Q_SLOTS:
    void syncAccounts(const QList<CDTpAccountPtr> &accounts);
    void createAccount(CDTpAccountPtr accountWrapper);
    void updateAccount(CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes);
    void removeAccount(CDTpAccountPtr accountWrapper);
    void syncAccountContacts(CDTpAccountPtr accountWrapper);
    void syncAccountContacts(CDTpAccountPtr accountWrapper,
            const QList<CDTpContactPtr> &contactsAdded,
            const QList<CDTpContactPtr> &contactsRemoved);
    void updateContact(CDTpContactPtr contactWrapper, CDTpContact::Changes changes);

public:
    void removeAccountContacts(const QString &accountPath, const QStringList &contactIds);

private Q_SLOTS:
    void onSyncOperationEnded(CDTpSparqlQuery *query);
    void onUpdateQueueTimeout();
    void onUpdateFinished(CDTpSparqlQuery *query);

private:
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mUpdateTimer;
    bool mUpdateRunning;
};

#endif // CDTPSTORAGE_H
