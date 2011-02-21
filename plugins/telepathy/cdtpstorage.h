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
    void syncAccount(CDTpAccountPtr accountWrapper);
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

private:
    QString saveAccountAvatar(CDTpAccountPtr accountWrapper) const;

    void addSyncAccountsToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpAccountPtr> &accounts) const;
    void addCreateAccountsToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpAccountPtr> &accounts) const;
    void addRemoveAccountsChangesToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const QString &imAccount,
            CDTpAccount::Changes changes) const;
    void addAccountChangesToBuilder(CDTpQueryBuilder &builder,
            CDTpAccountPtr accountWrapper,
            CDTpAccount::Changes changes) const;
    void addSyncNoRosterAccountsContactsToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpAccountPtr> accounts) const;
    void addSyncRosterAccountsContactsToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpAccountPtr> &accounts) const;
    void addCreateContactsToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpContactPtr> &contacts) const;
    void addRemoveContactsChangesToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpAccountPtr> &accounts) const;
    void addRemoveContactsChangesToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const QString &imContact,
            CDTpContact::Changes changes) const;
    void addContactChangesToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const QString &imContact,
            CDTpContact::Changes changes,
            Tp::ContactPtr contact) const;
    void addContactChangesToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            CDTpContact::Changes changes,
            Tp::ContactPtr contact) const;
    void addRemovePresenceToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress) const;
    void addPresenceToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const Tp::Presence &presence) const;
    void addRemoveCapabilitiesToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress) const;
    void addCapabilitiesToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            Tp::CapabilitiesBase capabilities) const;
    void addRemoveAvatarToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress) const;
    void addAvatarToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const QString &fileName) const;
    void addContactInfoToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const QString &imContact,
            Tp::ContactPtr contact) const;
    QString ensureContactAffiliationToBuilder(CDTpQueryBuilder &builder,
            const QString &imContact,
            const QString &graph,
            const Tp::ContactInfoField &field,
            QHash<QString, QString> &affiliations) const;
    void addRemoveContactsToBuilder(CDTpQueryBuilder &builder,
            CDTpAccountPtr accountWrapper,
            const QList<CDTpContactPtr> &contacts) const;
    void addRemoveContactsToBuilder(CDTpQueryBuilder &builder,
            const QString &accountPath,
            const QStringList &contactIds) const;
    void addRemoveContactToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress) const;
    void addRemoveContactInfoToBuilder(CDTpQueryBuilder &builder,
            const QString &imContact,
            const QString &graph) const;
    void addSetContactsUnknownToBuilder(CDTpQueryBuilder &builder,
            CDTpAccountPtr accountWrapper) const;

    QString presenceType(Tp::ConnectionPresenceType presenceType) const;
    QString presenceState(Tp::Contact::PresenceState presenceState) const;
    QString literal(const QString &str) const;
    QString literal(const QDateTime &dateTimeValue) const;
    QString literalTimeStamp() const;
    QString literalIMAddress(const QString &accountPath, const QString &contactId) const;
    QString literalIMAddress(const CDTpContactPtr &contactWrapper) const;
    QString literalIMAddress(const CDTpAccountPtr &accountWrapper) const;
    QString literalIMAccount(const QString &accountPath) const;
    QString literalIMAccount(const CDTpAccountPtr &accountWrapper) const;
    QString literalContactInfo(const Tp::ContactInfoField &field, int i) const;

private:
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mUpdateTimer;
};

#endif // CDTPSTORAGE_H
