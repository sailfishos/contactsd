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
    QString saveAccountAvatar(CDTpAccountPtr accountWrapper) const;

    CDTpQueryBuilder createAccountsBuilder(const QList<CDTpAccountPtr> &accounts) const;
    void addAccountChangesToBuilder(CDTpQueryBuilder &builder,
            CDTpAccountPtr accountWrapper,
            CDTpAccount::Changes changes) const;
    CDTpQueryBuilder syncNoRosterAccountsContactsBuilder(const QList<CDTpAccountPtr> accounts) const;
    CDTpQueryBuilder syncRosterAccountsContactsBuilder(const QList<CDTpAccountPtr> &accounts,
            bool purgeContacts = false) const;
    CDTpQueryBuilder createContactsBuilder(const QList<CDTpContactPtr> &contacts) const;
    void addContactChangesToBuilder(CDTpQueryBuilder &builder,
            const Value &imAddress,
            CDTpContact::Changes changes,
            Tp::ContactPtr contact) const;
    void addPresenceToBuilder(CDTpQueryBuilder &builder,
            const Value &imAddress,
            const Tp::Presence &presence) const;
    void addRemoveCapabilitiesToBuilder(CDTpQueryBuilder &builder,
            const Value &imAddress) const;
    void addCapabilitiesToBuilder(CDTpQueryBuilder &builder,
            const Value &imAddress,
            Tp::CapabilitiesBase capabilities) const;
    void addAvatarToBuilder(CDTpQueryBuilder &builder,
            const Value &imAddress,
            const QString &fileName) const;
    CDTpQueryBuilder createContactInfoBuilder(CDTpContactPtr contactWrapper) const;
    Value ensureContactAffiliationToBuilder(CDTpQueryBuilder &builder,
            const Value &imContact,
            const Value &graph,
            const Tp::ContactInfoField &field,
            QHash<QString, Value> &affiliations) const;
    CDTpQueryBuilder removeContactsBuilder(CDTpAccountPtr accountWrapper,
            const QList<CDTpContactPtr> &contacts) const;
    CDTpQueryBuilder removeContactsBuilder(const QString &accountPath,
            const QStringList &contactIds) const;
    CDTpQueryBuilder purgeContactsBuilder() const;
    void addRemoveContactInfoToBuilder(CDTpQueryBuilder &builder,
            const Value &imAddress,
            const Value &imContact) const;

    Value presenceType(Tp::ConnectionPresenceType presenceType) const;
    Value presenceState(Tp::Contact::PresenceState presenceState) const;
    Value literalTimeStamp() const;
    Value literalIMAddress(const QString &accountPath, const QString &contactId, bool resource = true) const;
    Value literalIMAddress(const CDTpContactPtr &contactWrapper, bool resource = true) const;
    Value literalIMAddress(const CDTpAccountPtr &accountWrapper, bool resource = true) const;
    Value literalIMAddressList(const QList<CDTpContactPtr> &contacts) const;
    Value literalIMAddressList(const QList<CDTpAccountPtr> &accounts) const;
    Value literalIMAccount(const QString &accountPath, bool resource = true) const;
    Value literalIMAccount(const CDTpAccountPtr &accountWrapper, bool resource = true) const;
    Value literalIMAccountList(const QList<CDTpAccountPtr> &accounts) const;
    Value literalContactInfo(const Tp::ContactInfoField &field, int i) const;

private:
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mUpdateTimer;
    bool mUpdateRunning;
};

#endif // CDTPSTORAGE_H
