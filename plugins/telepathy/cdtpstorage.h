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
    void syncAccount(CDTpAccount *accountWrapper);
    void syncAccount(CDTpAccount *accountWrapper, CDTpAccount::Changes changes);
    void syncAccountContacts(CDTpAccount *accountWrapper);
    void syncAccountContacts(CDTpAccount *accountWrapper,
            const QList<CDTpContactPtr> &contactsAdded,
            const QList<CDTpContactPtr> &contactsRemoved);
    void syncAccountContact(CDTpAccount *accountWrapper,
            CDTpContactPtr contactWrapper, CDTpContact::Changes changes);
    void setAccountContactsOffline(CDTpAccount *accountWrapper);
    void removeAccount(const QString &accountObjectPath);
    void removeContacts(CDTpAccount *accountWrapper,
            const QList<CDTpContactPtr> &contacts, bool not_ = false);

private Q_SLOTS:
    void onAccountPurgeSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onAccountOfflineSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onAccountDeleteSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onContactDeleteSelectQueryFinished(CDTpStorageSelectQuery *query);
    void onContactAddResolverFinished(CDTpStorageContactResolver *resolver);
    void onContactUpdateResolverFinished(CDTpStorageContactResolver *resolver);

private:
    void removeContacts(CDTpStorageSelectQuery *query, bool deleteAccount);

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
            CDTpAccount *accountWrapper,
            CDTpContactPtr contactWrapper);
    void addContactAuthorizationInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &lists,
            const RDFVariable &imAddress,
            CDTpContactPtr contactWrapper);
    void addContactInfoToQuery(RDFStatementList &inserts,
            RDFVariableList &imContactPropertyList,
            const RDFVariable &imContact,
            CDTpContactPtr contactWrapper);
    void addContactVoicePhoneNumberToQuery(RDFStatementList &inserts,
            RDFVariableList &properties,
            const RDFVariable &imAffiliation,
            const QString &phoneNumber);
    void addContactAddressToQuery(RDFStatementList &inserts,
            RDFVariableList &properties,
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
            Tp::ContactInfoField &field);
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
            const QList<CDTpContactPtr> &contactsToResolve,
             QObject *parent = 0);
    ~CDTpStorageContactResolver();

    QList<CDTpContactPtr> resolvedRemoteContacts() const;
    QList<CDTpContactPtr> remoteContacts() const;
    QString storageIdForContact(CDTpContactPtr contactWrapper) const;
    void setContactChanges(CDTpContact::Changes changes);
    CDTpContact::Changes contactChanges() const;

Q_SIGNALS:
   void finished(CDTpStorageContactResolver *resolveWrapper);

private Q_SLOTS:
    void onStorageResolveSelectQueryFinished(CDTpStorageSelectQuery *queryWrapper);

private:
    void requestContactResolve(CDTpAccount *accountWrapper,
            const QList<CDTpContactPtr> &contactsToResolve);
    QHash<CDTpContactPtr, QString> mResolvedContacts;
    QList<CDTpContactPtr> mContactsNotResolved;
    CDTpContact::Changes mContactChanges;
};

#endif // CDTPSTORAGE_H
