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
#include <QtSparql/QSparqlQuery>

#include "cdtpaccount.h"
#include "cdtpcontact.h"
#include "cdtpquery.h"

using namespace SopranoLive;

class CDTpStorageSyncOperations;
class CDTpStorageBuilder;

class CDTpStorage : public QObject
{
    Q_OBJECT

public:
    CDTpStorage(QObject *parent = 0);
    ~CDTpStorage();

    static const QString literalDefaultGraph;
    static const QString literalPrivateGraph;

    //FIXME: cleanup
    static const QString defaultGraph;
    static const QString privateGraph;
    static QUrl contactImAddress(const QString &contactAccountObjectPath,
            const QString &contactId);
    static QUrl contactImAddress(CDTpContactPtr contactWrapper);

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
    void onQueueTimerTimeout();
    void onAccountsSparqlQueryFinished(CDTpSparqlQuery *query);

    //FIXME: cleanup
    void onAccountPurgeSelectQueryFinished(CDTpSelectQuery *query);
    void onAccountDeleteSelectQueryFinished(CDTpSelectQuery *query);
    void onContactPurgeSelectQueryFinished(CDTpSelectQuery *);
    void onContactDeleteSelectQueryFinished(CDTpSelectQuery *query);
    void onAccountsUpdateQueryFinished(CDTpUpdateQuery *query);

private:
    void queueContactUpdate(CDTpContactPtr contactWrapper, CDTpContact::Changes);
    void updateQueuedContacts();
    void addContactAliasToBuilder(CDTpStorageBuilder &builder,
            const QString &imAddress,
            CDTpContactPtr contactWrapper) const;
    void addPresenceToBuilder(CDTpStorageBuilder &builder,
            const QString &imAddress,
            const Tp::Presence &presence) const;
    void addCapabilitiesToBuilder(CDTpStorageBuilder &builder,
            const QString &imAddress,
            Tp::CapabilitiesBase capabilities) const;
    void addContactAvatarToBuilder(CDTpStorageBuilder &builder,
            const QString &imAddress,
            CDTpContactPtr contactWrapper) const;
    void addContactAuthorizationToBuilder(CDTpStorageBuilder &builder,
            const QString &imAddress,
            CDTpContactPtr contactWrapper) const;
    void addContactInfoToBuilder(CDTpStorageBuilder &builder,
            const QString &imAddress,
            const QString &imContact,
            CDTpContactPtr contactWrapper) const;
    QString ensureContactAffiliationToBuilder(CDTpStorageBuilder &builder,
            const QString &imContact,
            const QString &graph,
            const Tp::ContactInfoField &field,
            QHash<QString, QString> &affiliations) const;
    void addRemoveContactInfoToBuilder(CDTpStorageBuilder &builder,
            const QString &imContact,
            const QString &graph) const;
    void addAccountAvatarToBuilder(CDTpStorageBuilder &builder,
            const QString &imAddress,
            const Tp::Avatar &avatar) const;

    void oneSyncOperationFinished(CDTpAccountPtr accountWrapper);

    QString presenceType(Tp::ConnectionPresenceType presenceType) const;
    QString presenceState(Tp::Contact::PresenceState presenceState) const;
    QString literal(const QString &str) const;
    QString literal(const QDateTime &dateTimeValue) const;
    QString literalTimeStamp() const;
    QString literalIMAddress(const QString &accountPath, const QString &contactId) const;
    QString literalIMAddress(const CDTpContactPtr &contactWrapper) const;
    QString literalIMAddress(const CDTpAccountPtr &accountWrapper) const;
    QString literalIMAccount(const CDTpAccountPtr &accountWrapper) const;
    QString literalContactInfo(const Tp::ContactInfoField &field, int i) const;

    //FIXME: cleanup
    void addRemoveContactToQuery(RDFUpdate &query,
            RDFStatementList &inserts,
            RDFStatementList &deletions,
            const CDTpContactsSelectItem &item);
    void addRemoveContactFromAccountToQuery(RDFStatementList &deletions,
            const CDTpContactsSelectItem &item);
    void addRemoveContactInfoToQuery(RDFUpdate &query,
            const RDFVariable &imContact,
            const QUrl &graph);

private:
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mQueueTimer;
    QHash<CDTpAccountPtr, CDTpStorageSyncOperations> mSyncOperations;
};

class CDTpStorageBuilder
{
public:
    CDTpStorageBuilder();
    void createResource(const QString &resource, const QString &type, const QString &graph = CDTpStorage::literalDefaultGraph);
    void insertProperty(const QString &resource, const QString &property, const QString &value, const QString &graph = CDTpStorage::literalDefaultGraph);
    void deleteResource(const QString &resource);
    QString deleteProperty(const QString &resource, const QString &property);
    QString deleteProperty(const QString &resource, const QString &property, const QString &graph);
    QString deletePropertyAndLinkedResource(const QString &resource, const QString &property);
    QString updateProperty(const QString &resource, const QString &property, const QString &value, const QString &graph = CDTpStorage::literalDefaultGraph);

    void addCustomSelection(const QString &selection);

    QString uniquify(const QString &v = QString("?v"));
    QString getRawQuery() const;
    QSparqlQuery getSparqlQuery() const;
    void clear();

private:
    QString join(const QStringList &lines, const QString &indent) const;

    QHash<QString, QStringList> insertPart;
    QStringList insertPartWhere;
    QStringList deletePart;
    QStringList deletePartWhere;
    QStringList customSelection;
    int vCount;
};

struct CDTpStorageSyncOperations
{
    CDTpStorageSyncOperations();
    bool active;
    int nPendingOperations;
    int nContactsAdded;
    int nContactsRemoved;
};

#endif // CDTPSTORAGE_H
