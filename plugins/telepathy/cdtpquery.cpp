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

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nie.h>
#include <QtTracker/Tracker>

#include "cdtpquery.h"
#include "cdtpstorage.h"

using namespace SopranoLive;

/* --- CDTpSelectQuery --- */

CDTpSelectQuery::CDTpSelectQuery(const RDFSelect &select, QObject *parent)
    : QObject(parent)
{
    setSelect(select);
}

CDTpSelectQuery::CDTpSelectQuery(QObject *parent) : QObject(parent)
{
}

void CDTpSelectQuery::setSelect(const RDFSelect &select)
{
    mReply = ::tracker()->modelQuery(select);
    connect(mReply.model(),
            SIGNAL(modelUpdated()),
            SLOT(onModelUpdated()));
}

void CDTpSelectQuery::onModelUpdated()
{
    Q_EMIT finished(this);
    deleteLater();
}

/* --- CDTpContactsSelectQuery --- */

CDTpContactsSelectQuery::CDTpContactsSelectQuery(const QList<CDTpContactPtr> &contacts, QObject *parent)
    : CDTpSelectQuery(parent), mReplyParsed(false)
{
    setSelect(contacts);
}

CDTpContactsSelectQuery::CDTpContactsSelectQuery(const QString &accountObjectPath, QObject *parent)
    : CDTpSelectQuery(parent), mReplyParsed(false)
{
    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAffiliation = imContact.property<nco::hasAffiliation>();
    RDFVariable imAddress = imAffiliation.property<nco::hasIMAddress>();
    imAddress.hasPrefix(QString("telepathy:%1").arg(accountObjectPath));

    setSelect(imContact, imAffiliation, imAddress);
}

CDTpContactsSelectQuery::CDTpContactsSelectQuery(QObject *parent)
    : CDTpSelectQuery(parent), mReplyParsed(false)
{
}

QList<CDTpContactsSelectItem> CDTpContactsSelectQuery::items()
{
    ensureParsed();
    return mItems;
}

void CDTpContactsSelectQuery::setSelect(const QList<CDTpContactPtr> &contacts)
{
    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAffiliation = imContact.property<nco::hasAffiliation>();
    RDFVariable imAddress = imAffiliation.property<nco::hasIMAddress>();
    imContact.notEqual(nco::default_contact_me::iri());

    RDFVariableList members;
    Q_FOREACH (CDTpContactPtr contactWrapper, contacts) {
        members << RDFVariable(CDTpStorage::contactImAddress(contactWrapper));
    }
    imAddress.isMemberOf(members);

    setSelect(imContact, imAffiliation, imAddress);
}

void CDTpContactsSelectQuery::setSelect(const RDFVariable &imContact,
    const RDFVariable &imAffiliation, const RDFVariable &imAddress)
{
    RDFSelect select;
    select.addColumn("contact", imContact);
    select.addColumn("affiliation", imAffiliation);
    select.addColumn("address", imAddress);
    select.addColumn("generator", imContact.property<nie::generator>());
    select.addColumn("localUID", imContact.property<nco::contactLocalUID>());

    CDTpSelectQuery::setSelect(select);
}

void CDTpContactsSelectQuery::ensureParsed()
{
    if (mReplyParsed) {
        return;
    }

    LiveNodes result = reply();
    for (int i = 0; i < result->rowCount(); i++) {
        CDTpContactsSelectItem item;
        item.imContact     = result->index(i, 0).data().toString();
        item.imAffiliation = result->index(i, 1).data().toString();
        item.imAddress     = result->index(i, 2).data().toString();
        item.generator     = result->index(i, 3).data().toString();
        item.localUID      = result->index(i, 4).data().toString();

        mItems << item;
    }

    mReplyParsed = true;
}

/* --- CDTpAccountContactsSelectQuery --- */

CDTpAccountContactsSelectQuery::CDTpAccountContactsSelectQuery(CDTpAccountPtr accountWrapper, QObject *parent)
    : CDTpContactsSelectQuery(parent), mAccountWrapper(accountWrapper)
{
    const QString accountId = accountWrapper->account()->normalizedName();
    const QString accountPath = accountWrapper->account()->objectPath();

    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAffiliation = imContact.property<nco::hasAffiliation>();
    RDFVariable imAddress = imAffiliation.property<nco::hasIMAddress>();
    imAddress.hasPrefix(QString("telepathy:%1").arg(accountPath));
    imAddress.notEqual(CDTpStorage::contactImAddress(accountPath, accountId));
    imContact.notEqual(nco::default_contact_me::iri());

    setSelect(imContact, imAffiliation, imAddress);
}

/* --- CDTpContactResolver --- */

CDTpContactResolver::CDTpContactResolver(
        const QHash<CDTpContactPtr, CDTpContact::Changes> &contactsToResolve,
        QObject *parent)
    : CDTpContactsSelectQuery(parent), mReplyParsed(false),
      mContacts(contactsToResolve)
{
    setSelect(remoteContacts());
}

QList<CDTpContactPtr> CDTpContactResolver::remoteContacts() const
{
    return mContacts.keys();
}

CDTpContact::Changes CDTpContactResolver::contactChanges(const CDTpContactPtr &contactWrapper) const
{
    return mContacts[contactWrapper];
}

QString CDTpContactResolver::storageIdForContact(const CDTpContactPtr &contactWrapper)
{
    ensureParsed();
    return mResolvedContacts[contactWrapper];
}

void CDTpContactResolver::ensureParsed()
{
    if (mReplyParsed) {
        return;
    }

    QList<CDTpContactPtr> contactsToResolve = remoteContacts();
    Q_FOREACH (const CDTpContactsSelectItem &item, items()) {
        Q_FOREACH (CDTpContactPtr contactWrapper, contactsToResolve) {
            if (CDTpStorage::contactImAddress(contactWrapper) == item.imAddress) {
                mResolvedContacts[contactWrapper] = item.localUID;
            }
        }
    }

    mReplyParsed = true;
}

/* --- CDTpUpdateQuery --- */

CDTpUpdateQuery::CDTpUpdateQuery(RDFUpdate &updateQuery, QObject *parent)
    : QObject(parent), mSparql(updateQuery.getQuery())
{
    mTransaction = ::tracker()->createTransaction();
    connect(mTransaction.data(),
        SIGNAL(commitFinished()),
        SLOT(onCommitFinished()));
    connect(mTransaction.data(),
        SIGNAL(commitError(QString)),
        SLOT(onCommitError(QString)));

    ::tracker()->executeQuery(updateQuery);
    mTransaction->commit();
}

void CDTpUpdateQuery::onCommitFinished()
{
    Q_EMIT finished(this);
    deleteLater();
}

void CDTpUpdateQuery::onCommitError(QString message)
{
    qDebug() << "query finished with error" << message;
    qDebug() << mSparql;
    Q_EMIT finished(this);
    deleteLater();
}

/* --- CDTpAccountsUpdateQuery --- */

CDTpAccountsUpdateQuery::CDTpAccountsUpdateQuery(const QList<CDTpAccountPtr> &accounts,
    RDFUpdate &updateQuery, QObject *parent)
        : CDTpUpdateQuery(updateQuery, parent), mAccounts(accounts)
{
}
