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

#include <QContact>
#include <QContactOnlineAccount>

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QNetworkAccessManager>

#include "cdtpaccount.h"
#include "cdtpcontact.h"

QTCONTACTS_USE_NAMESPACE

class CDTpDevicePresence;
class CDTpStorage : public QObject
{
    Q_OBJECT

public:
    typedef QMap<CDTpContact::Changes, QList<QContact> > ContactChangeSet;

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
    void reportPresenceStates();

public:
    void createAccountContacts(CDTpAccountPtr accountWrapper, const QStringList &imIds, uint localId);
    void removeAccountContacts(CDTpAccountPtr accountWrapper, const QStringList &contactIds);

private Q_SLOTS:
    void onUpdateQueueTimeout();

    void addNewAccount();
    void updateAccount();

private:
    void cancelQueuedUpdates(const QList<CDTpContactPtr> &contacts);

    void addNewAccount(QContact &self, CDTpAccountPtr accountWrapper);
    void removeExistingAccount(QContact &self, QContactOnlineAccount &existing);

    void updateAccountChanges(QContact &self, QContactOnlineAccount &qcoa, CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes);

    bool initializeNewContact(QContact &newContact, CDTpAccountPtr accountWrapper, const QString &contactId, const QString &alias);
    bool initializeNewContact(QContact &newContact, CDTpContactPtr contactWrapper);

    void updateContactChanges(CDTpContactPtr contactWrapper, CDTpContact::Changes changes, QContact &existing, ContactChangeSet *saveList,
                              QList<QContactId> *removeList);
    void updateContactChanges(CDTpContactPtr contactWrapper, CDTpContact::Changes changes);

private:
    QNetworkAccessManager mNetwork;
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mUpdateTimer;
    QElapsedTimer mWaitTimer;
    QMap<QString, CDTpAccount::Changes> m_accountPendingChanges;
    CDTpDevicePresence *mDevicePresence;
};

#endif // CDTPSTORAGE_H
