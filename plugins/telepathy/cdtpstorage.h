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
    static QString contactLocalId(CDTpContact *contactWrapper);

    static QUrl contactIri(const QString &contactLocalId);
    static QUrl contactIri(CDTpContact *contactWrapper);

    static QUrl contactImAddress(const QString &contactAccountObjectPath,
            const QString &contactId);
    static QUrl contactImAddress(CDTpContact *contactWrapper);

    static QUrl trackerStatusFromTpPresenceType(uint tpPresenceType);
    static QUrl trackerStatusFromTpPresenceStatus(
            const QString &tpPresenceStatus);
    static QUrl authStatus(Tp::Contact::PresenceState);

    static const QUrl defaultGraph;

public Q_SLOTS:
    void syncAccountSet(const QList<QString> &accountPaths);
    void syncAccount(CDTpAccount *accountWrapper);
    void syncAccount(CDTpAccount *accountWrapper, CDTpAccount::Changes changes);
    void syncAccountContacts(CDTpAccount *accountWrapper);
    void syncAccountContacts(CDTpAccount *accountWrapper,
            const QList<CDTpContact *> &contactsAdded,
            const QList<CDTpContact *> &contactsRemoved);
    void syncAccountContact(CDTpAccount *accountWrapper,
            CDTpContact *contactWrapper, CDTpContact::Changes changes);
    void setAccountContactsOffline(CDTpAccount *accountWrapper);
    void removeAccount(const QString &accountObjectPath);

private Q_SLOTS:
    void onAccountPurgeSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onAccountOfflineSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onContactAddResolverFinished(CDTpStorageContactResolver *resolver);
    void onContactDeleteResolverFinished(CDTpStorageContactResolver *resolver);
    void onContactUpdateResolverFinished(CDTpStorageContactResolver *resolver);

private:
    void saveAccountAvatar(RDFUpdate &query, const QByteArray &data, const QString &mimeType,
            const RDFVariable &imAddress,
            RDFStatementList &inserts);

    void addContactAliasInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);
    void addContactPresenceInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);
    void addContactCapabilitiesInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);
    void addContactAvatarInfoToQuery(RDFUpdate &query,
            RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);
    void addContactRemoveInfoToQuery(RDFStatementList &deletions,
            RDFStatementList &inserts,
            const QString &contactId,
            CDTpAccount *accountWrapper,
            CDTpContact *contactWrapper);
    void addContactAuthorizationInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);
    void addContactInfoToQuery(RDFUpdate &query,
            RDFStatementList &inserts,
            RDFVariableList &imAddressPropertyList,
            RDFVariableList &imContactPropertyList,
            const RDFVariable &imAddress,
            const RDFVariable &imContact,
            CDTpContact *contactWrapper);
    void addContactVoicePhoneNumberToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const QString &phoneNumber,
            const QString &affiliation,
            const RDFVariable &imContact);

    void addContactAddressToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const QString &pobox,
            const QString &extAddress,
            const QString &streetAddress,
            const QString &locality,
            const QString &region,
            const QString &postalcode,
            const QString &country,
            const QString &affiliation,
            const RDFVariable &imContact);

    QString contactLocalId(const QString &contactAccountObjectPath,
            const QString &contactId) const;
    QString contactLocalId(CDTpContact *contactWrapper) const;

    QUrl contactIri(const QString &contactLocalId) const;
    QUrl contactIri(CDTpContact *contactWrapper) const;

    QUrl contactImAddress(const QString &contactAccountObjectPath,
            const QString &contactId) const;
    QUrl contactImAddress(CDTpContact *contactWrapper) const;

    QUrl trackerStatusFromTpPresenceType(uint tpPresenceType) const;
    QUrl trackerStatusFromTpPresenceStatus(
            const QString &tpPresenceStatus) const;
    QUrl authStatus(Tp::Contact::PresenceState) const;

    static const QUrl defaultGraph;
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

class CDTpStorageContactResolver : public QObject
{
    Q_OBJECT

public:
    CDTpStorageContactResolver(CDTpAccount *accountWrapper,
            const QList<CDTpContact *> &contactsToResolve,
             QObject *parent = 0);
    ~CDTpStorageContactResolver();

    QList<CDTpContact *> resolvedRemoteContacts() const;
    QList<CDTpContact *> remoteContacts() const;
    QString storageIdForContact(CDTpContact *contactWrapper) const;
    void setContactChanges(CDTpContact::Changes changes);
    CDTpContact::Changes contactChanges() const;

Q_SIGNALS:
   void finished(CDTpStorageContactResolver *resolveWrapper);

private Q_SLOTS:
    void onStorageResolveSelectQueryFinished(CDTpStorageSelectQuery *queryWrapper);

private:
    void requestContactResolve(CDTpAccount *accountWrapper,
            const QList<CDTpContact *> &contactsToResolve);
    QHash<CDTpContact *, QString> mResolvedContacts;
    QList<CDTpContact *> mContactsNotResolved;
    CDTpContact::Changes mContactChanges;
};

class CDTpStorageRemoveAccount : public QObject
{
    Q_OBJECT

public:
    CDTpStorageRemoveAccount(const QString &accountObjectPath, QObject *parent = 0);
    ~CDTpStorageRemoveAccount() {};

private Q_SLOTS:
    void onSelectQueryFinished(CDTpStorageSelectQuery *query);

private:
    QString mAccountObjectPath;
};

#endif // CDTPSTORAGE_H
