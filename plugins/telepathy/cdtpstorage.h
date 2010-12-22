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

#include "cdtpaccount.h"
#include "cdtpcontact.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nie.h>
#include <QtTracker/QLive>
#include <QtTracker/Tracker>

using namespace SopranoLive;

class CDTpStorageSelectQuery;
class CDTpStorageContactResolver;
class CDTpStorageSelectAccountsToDelete;

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

    static QUrl trackerStatusFromTpPresenceType(uint tpPresenceType);
    static QUrl trackerStatusFromTpPresenceStatus(
            const QString &tpPresenceStatus);
    static QUrl authStatus(Tp::Contact::PresenceState);

    static const QUrl defaultGraph;
    static const QUrl privateGraph;

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
    void onAccountPurgeSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onAccountOfflineSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onAccountDeleteSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onContactPurgeSelectQueryFinished(CDTpStorageSelectQuery *);
    void onContactDeleteSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onContactUpdateResolverFinished(CDTpStorageContactResolver *resolver);
    void onQueueTimerTimeout();

private:
    void removeContacts(CDTpStorageSelectQuery *query, bool deleteAccount,
            QList<QUrl> skipIMAddressList = QList<QUrl>());

    void saveAccountAvatar(RDFUpdate &query, const QByteArray &data, const QString &mimeType,
            const RDFVariable &imAddress,
            RDFStatementList &inserts);

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
    void addContactInfoToQuery(RDFUpdate &query,
            const RDFVariable &imContact,
            CDTpContactPtr contactWrapper);
    void addContactVoicePhoneNumberToQuery(RDFStatementList &inserts,
            const RDFVariable &imAffiliation,
            const QString &phoneNumber);
    void addContactAddressToQuery(RDFStatementList &inserts,
            const RDFVariable &imAffiliation,
            const QString &pobox,
            const QString &extendedAddress,
            const QString &streetAddress,
            const QString &locality,
            const QString &region,
            const QString &postalcode,
            const QString &country);
    RDFVariable createAffiliation(RDFStatementList &inserts,
            const RDFVariable &imContact,
            const Tp::ContactInfoField &field);

    void queueUpdate(CDTpContactPtr contactWrapper, CDTpContact::Changes);


    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mQueueTimer;
};

class CDTpStorageSelectQuery : public QObject
{
    Q_OBJECT

public:
    CDTpStorageSelectQuery(const RDFSelect &select, QObject *parent = 0);
    ~CDTpStorageSelectQuery();

    LiveNodes reply() const { return mReply; }

Q_SIGNALS:
    void finished(CDTpStorageSelectQuery *queryWrapper);

private Q_SLOTS:
    void onModelUpdated();

private:
    LiveNodes mReply;
};

class CDTpStorageAccountSelectQuery: public CDTpStorageSelectQuery
{
    Q_OBJECT

public:
    CDTpStorageAccountSelectQuery(CDTpAccountPtr accountWrapper,
            const RDFSelect &select, QObject *parent = 0);
    ~CDTpStorageAccountSelectQuery() {};

    CDTpAccountPtr accountWrapper() const;

private:
    CDTpAccountPtr mAccountWrapper;
};

class CDTpStorageContactResolver : public QObject
{
    Q_OBJECT

public:
    CDTpStorageContactResolver(
            const QHash<CDTpContactPtr, CDTpContact::Changes> &contactsToResolve,
            QObject *parent = 0);
    ~CDTpStorageContactResolver();

    QList<CDTpContactPtr> remoteContacts() const;
    QString storageIdForContact(const CDTpContactPtr &contactWrapper) const;
    CDTpContact::Changes contactChanges(const CDTpContactPtr &contactWrapper) const;

Q_SIGNALS:
   void finished(CDTpStorageContactResolver *resolveWrapper);

private Q_SLOTS:
    void onStorageResolveSelectQueryFinished(CDTpStorageSelectQuery *queryWrapper);

private:
    QHash<CDTpContactPtr, QString> mResolvedContacts;
    QHash<CDTpContactPtr, CDTpContact::Changes> mContacts;
};

#endif // CDTPSTORAGE_H
