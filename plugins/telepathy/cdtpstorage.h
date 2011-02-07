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

class CDTpStorageSyncOperations;

class CDTpStorage : public QObject
{
    Q_OBJECT

public:
    CDTpStorage(QObject *parent = 0);
    ~CDTpStorage();

    static const QString defaultGraph;
    static const QString privateGraph;
    static const QString defaultGenerator;

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

private Q_SLOTS:
    void onQueueTimerTimeout();

private:
    void queueContactUpdate(CDTpContactPtr contactWrapper, CDTpContact::Changes changes);

    void addCreateAccountsToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpAccountPtr> &accounts) const;
    void addUpdateAccountToBuilder(CDTpQueryBuilder &builder,
            CDTpAccountPtr accountWrapper,
            CDTpAccount::Changes changes) const;
    void addSyncAccountContactsToBuilder(CDTpQueryBuilder &builder,
            CDTpAccountPtr accountWrapper) const;
    void addCreateContactsToBuilder(CDTpQueryBuilder &builder,
            const QList<CDTpContactPtr> &contacts) const;
    void addUpdateContactToBuilder(CDTpQueryBuilder &builder,
            CDTpContactPtr contactWrapper,
            CDTpContact::Changes changes) const;
    void addContactAliasToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            CDTpContactPtr contactWrapper) const;
    void addPresenceToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const Tp::Presence &presence) const;
    void addCapabilitiesToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            Tp::CapabilitiesBase capabilities) const;
    void addContactAvatarToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            CDTpContactPtr contactWrapper) const;
    void addAccountAvatarToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress, const Tp::Avatar &avatar) const;
    void addContactAuthorizationToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            CDTpContactPtr contactWrapper) const;
    void addContactInfoToBuilder(CDTpQueryBuilder &builder,
            const QString &imAddress,
            const QString &imContact,
            CDTpContactPtr contactWrapper) const;
    QString ensureContactAffiliationToBuilder(CDTpQueryBuilder &builder,
            const QString &imContact,
            const QString &graph,
            const Tp::ContactInfoField &field,
            QHash<QString, QString> &affiliations) const;
    void addRemoveContactsToBuilder(CDTpQueryBuilder &builder,
            CDTpAccountPtr accountWrapper,
            const QList<CDTpContactPtr> &contacts) const;
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
    QString literalIMAccount(const CDTpAccountPtr &accountWrapper) const;
    QString literalContactInfo(const Tp::ContactInfoField &field, int i) const;

private:
    QHash<CDTpContactPtr, CDTpContact::Changes> mUpdateQueue;
    QTimer mQueueTimer;
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
