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

#include <QtTracker/Tracker>

#include "cdtpaccount.h"
#include "cdtpcontact.h"
#include "cdtpquery.h"

using namespace SopranoLive;

class CDTpStorageSyncOperations;

class CDTpStorage : public QObject
{
    Q_OBJECT

public:
    CDTpStorage(QObject *parent = 0);
    ~CDTpStorage();

    static QString contactLocalId(const QString &contactAccountObjectPath,
            const QString &contactId);
    static QString contactLocalId(CDTpContactPtr contactWrapper);

    static QUrl contactIri(const QString &contactLocalId);
    static QUrl contactIri(CDTpContactPtr contactWrapper);

    static QUrl contactImAddress(const QString &contactAccountObjectPath,
            const QString &contactId);
    static QUrl contactImAddress(CDTpContactPtr contactWrapper);

    static QUrl contactAffiliation(const QString &contactAccountObjectPath,
            const QString &contactId);
    static QUrl contactAffiliation(CDTpContactPtr contactWrapper);

    static QUrl trackerStatusFromTpPresenceType(uint tpPresenceType);
    static QUrl trackerStatusFromTpPresenceStatus(
            const QString &tpPresenceStatus);
    static QUrl authStatus(Tp::Contact::PresenceState);

    static const QUrl defaultGraph;
    static const QUrl privateGraph;

Q_SIGNALS:
    void syncStarted(CDTpAccountPtr accountWrapper);
    void syncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved);

public Q_SLOTS:
    void syncAccountSet(const QList<QString> &accountPaths);
    void syncAccount(CDTpAccountPtr accountWrapper);
    void syncAccount(CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes);
    void syncAccountContacts(CDTpAccountPtr accountWrapper);
    void syncAccountContacts(CDTpAccountPtr accountWrapper,
            const QList<CDTpContactPtr> &contactsAdded,
            const QList<CDTpContactPtr> &contactsRemoved);
    void syncAccountContact(CDTpAccountPtr accountWrapper,
            CDTpContactPtr contactWrapper, CDTpContact::Changes changes);
    void setAccountContactsOffline(CDTpAccountPtr accountWrapper);
    void removeAccount(const QString &accountObjectPath);
    void removeContacts(CDTpAccountPtr accountWrapper,
            const QList<CDTpContactPtr> &contacts);

private Q_SLOTS:
    void onAccountPurgeSelectQueryFinished(CDTpSelectQuery *query);
    void onAccountOfflineSelectQueryFinished(CDTpSelectQuery *query);
    void onAccountDeleteSelectQueryFinished(CDTpSelectQuery *query);
    void onContactPurgeSelectQueryFinished(CDTpSelectQuery *);
    void onContactDeleteSelectQueryFinished(CDTpSelectQuery *query);
    void onContactUpdateSelectQueryFinished(CDTpSelectQuery *query);
    void onAccountsUpdateQueryFinished(CDTpUpdateQuery *query);
    void onQueueTimerTimeout();

private:
    void saveAccountAvatar(RDFUpdate &query, const QByteArray &data, const QString &mimeType,
            const RDFVariable &imAddress,
            RDFStatementList &inserts);

    void addRemoveContactToQuery(RDFUpdate &query,
            RDFStatementList &inserts,
            RDFStatementList &deletions,
            const CDTpContactsSelectItem &item);
    void addRemoveContactFromAccountToQuery(RDFStatementList &deletions,
            const CDTpContactsSelectItem &item);

    void addContactAliasInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContactPtr contactWrapper);
    void addContactPresenceInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContactPtr contactWrapper);
    void addContactCapabilitiesInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContactPtr contactWrapper);
    void addContactAvatarInfoToQuery(RDFUpdate &query,
            RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContactPtr contactWrapper);
    void addContactRemoveInfoToQuery(RDFStatementList &deletions,
            RDFStatementList &inserts,
            const QString &contactId,
            CDTpAccountPtr accountWrapper,
            CDTpContactPtr contactWrapper);
    void addContactAuthorizationInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContactPtr contactWrapper);
    void addRemoveContactInfoToQuery(RDFUpdate &query,
            const RDFVariable &imContact,
            const QUrl &graph);
    void addContactInfoToQuery(RDFUpdate &query,
            RDFStatementList &inserts,
            const RDFVariable &imContact,
            CDTpContactPtr contactWrapper);
    void addContactVoicePhoneNumberToQuery(RDFStatementList &graphInserts,
            RDFStatementList &inserts,
            const RDFVariable &affiliation,
            const QString &phoneNumber);
    void addContactAddressToQuery(RDFStatementList &graphInserts,
            const RDFVariable &affiliation,
            const QString &pobox,
            const QString &extendedAddress,
            const QString &streetAddress,
            const QString &locality,
            const QString &region,
            const QString &postalcode,
            const QString &country);
    void addContactEmailToQuery(RDFStatementList &graphInserts,
            RDFStatementList &inserts,
            const RDFVariable &affiliation,
            const QString &email);
    RDFVariable ensureAffiliation(QHash<QString, RDFVariable> &map,
            RDFStatementList &graphInserts,
            const RDFVariable &imContact,
            const Tp::ContactInfoField &field);

    void queueUpdate(CDTpContactPtr contactWrapper, CDTpContact::Changes);
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mQueueTimer;

    void oneSyncOperationFinished(CDTpAccountPtr accountWrapper);
    QHash<CDTpAccountPtr, CDTpStorageSyncOperations> mSyncOperations;
};

class CDTpStorageSyncOperations
{
public:
    CDTpStorageSyncOperations();
    bool active;
    int nPendingOperations;
    int nContactsAdded;
    int nContactsRemoved;
};

#endif // CDTPSTORAGE_H
