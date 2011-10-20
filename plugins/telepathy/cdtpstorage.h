/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#ifndef CDTPSTORAGE_H
#define CDTPSTORAGE_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

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
    void error(int code, const QString &message);

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
    void createAccountContacts(const QString &accountPath, const QStringList &imIds, uint localId);
    void removeAccountContacts(const QString &accountPath, const QStringList &contactIds);

private Q_SLOTS:
    void onSyncOperationEnded(CDTpSparqlQuery *query);
    void onUpdateQueueTimeout();
    void onUpdateFinished(CDTpSparqlQuery *query);
    void onSparqlQueryFinished(CDTpSparqlQuery *query);

private:
    void cancelQueuedUpdates(const QList<CDTpContactPtr> &contacts);
    void triggerGarbageCollector(CDTpQueryBuilder &builder, uint nContacts);

private:
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QNetworkAccessManager mNetwork;
    QTimer mUpdateTimer;
    bool mUpdateRunning;
    bool mDirectGC;
};

#endif // CDTPSTORAGE_H
