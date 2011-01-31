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

#include <TelepathyQt4/AvatarData>
#include <TelepathyQt4/ContactCapabilities>
#include <TelepathyQt4/ConnectionCapabilities>

#include <QtSparql>

#include "cdtpstorage.h"
#include "sparqlconnectionmanager.h"

#define MAX_UPDATE_SIZE 999
#define MAX_REMOVE_SIZE 999

const QUrl CDTpStorage::defaultGraph = QUrl(
        QLatin1String("urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9"));
const QUrl CDTpStorage::privateGraph = QUrl(
        QLatin1String("urn:uuid:679293d4-60f0-49c7-8d63-f1528fe31f66"));

const QString defaultGenerator = "telepathy";

CDTpStorage::CDTpStorage(QObject *parent)
    : QObject(parent)
{
    //::tracker()->setServiceAttribute("tracker_access_method", QString("QSPARQL_DIRECT"));

    mQueueTimer.setSingleShot(true);
    mQueueTimer.setInterval(100);
    connect(&mQueueTimer, SIGNAL(timeout()), SLOT(onQueueTimerTimeout()));
}

CDTpStorage::~CDTpStorage()
{
}

void CDTpStorage::syncAccountSet(const QList<QString> &accounts)
{
    RDFVariable imAccount = RDFVariable::fromType<nco::IMAccount>();

    RDFVariableList members;
    Q_FOREACH (QString accountObjectPath, accounts) {
        members << RDFVariable(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
    }
    imAccount.isMemberOf(members).not_();

    RDFSelect select;
    select.addColumn("Accounts", imAccount);

    CDTpSelectQuery *query = new CDTpSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpSelectQuery *)),
            SLOT(onAccountPurgeSelectQueryFinished(CDTpSelectQuery *)));
}

void CDTpStorage::onAccountPurgeSelectQueryFinished(CDTpSelectQuery *query)
{
    LiveNodes result = query->reply();

    for (int i = 0; i < result->rowCount(); i++) {
        const QString accountUrl = result->index(i, 0).data().toString();
        const QString accountObjectPath = accountUrl.mid(QString("telepathy:").length());

        removeAccount(accountObjectPath);
    }
}

void CDTpStorage::syncAccount(CDTpAccountPtr accountWrapper)
{
    syncAccount(accountWrapper, CDTpAccount::All);

    /* If contactsd leaves while account is still online, and get restarted
     * when account is offline then contacts in tracker still have presence.
     * This happens when rebooting the device. */
    if (!accountWrapper->account()->connection()) {
        setAccountContactsOffline(accountWrapper);
    }
}

// TODO: Improve syncAccount so that it only updates the data that really
//       changed
void CDTpStorage::syncAccount(CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes)
{
    Tp::AccountPtr account = accountWrapper->account();
    if (account->normalizedName().isEmpty()) {
        return;
    }

    const QString accountObjectPath = account->objectPath();
    const QString accountId = account->normalizedName();
    const QDateTime datetime = QDateTime::currentDateTime();

    qDebug() << "Syncing account" << accountObjectPath << "to storage" << accountId;

    RDFUpdate up;
    RDFStatementList inserts;

    // Create the IMAddress for this account's self contact
    RDFVariable imAddress(contactImAddress(accountObjectPath, accountId));
    inserts << RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri())
            << RDFStatement(imAddress, nco::imID::iri(), LiteralValue(account->normalizedName()))
            << RDFStatement(imAddress, nco::imProtocol::iri(), LiteralValue(account->protocolName()));

    if (changes & CDTpAccount::Nickname) {
        qDebug() << "  nickname changed";
        up.addDeletion(imAddress, nco::imNickname::iri(), RDFVariable(), defaultGraph);
        inserts << RDFStatement(imAddress, nco::imNickname::iri(), LiteralValue(account->nickname()));
    }

    if (changes & CDTpAccount::Presence) {
        qDebug() << "  presence changed";
        Tp::Presence presence = account->currentPresence();
        QUrl status = trackerStatusFromTpPresenceStatus(presence.status());

        up.addDeletion(imAddress, nco::imPresence::iri(), RDFVariable(), defaultGraph);
        up.addDeletion(imAddress, nco::imStatusMessage::iri(), RDFVariable(), defaultGraph);
        up.addDeletion(imAddress, nco::presenceLastModified::iri(), RDFVariable(), defaultGraph);

        inserts << RDFStatement(imAddress, nco::imPresence::iri(), RDFVariable(status))
                << RDFStatement(imAddress, nco::imStatusMessage::iri(), LiteralValue(presence.statusMessage()))
                << RDFStatement(imAddress, nco::presenceLastModified::iri(), LiteralValue(datetime));
    }

    if (changes & CDTpAccount::Avatar) {
        qDebug() << "  avatar changed";
        const Tp::Avatar &avatar = account->avatar();
        // TODO: saving to disk needs to be removed here
        saveAccountAvatar(up, avatar.avatarData, avatar.MIMEType, imAddress,
            inserts);
    }

    // Create an IMAccount
    RDFStatementList imAccountInserts;
    RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
    imAccountInserts << RDFStatement(imAccount, rdf::type::iri(), nco::IMAccount::iri())
                     << RDFStatement(imAccount, nco::imAccountType::iri(), LiteralValue(account->protocolName()))
                     << RDFStatement(imAccount, nco::imAccountAddress::iri(), imAddress)
                     << RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress);

    if (changes & CDTpAccount::DisplayName) {
        qDebug() << "  display name changed";
        up.addDeletion(imAccount, nco::imDisplayName::iri(), RDFVariable(), privateGraph);
        imAccountInserts << RDFStatement(imAccount, nco::imDisplayName::iri(), LiteralValue(account->displayName()));
    }

    // link the IMAddress to me-contact via an affiliation
    const QString strLocalUID = QString::number(0x7FFFFFFF);
    RDFVariable imAffiliation(contactAffiliation(accountObjectPath, accountId));
    inserts << RDFStatement(imAffiliation, rdf::type::iri(), nco::Affiliation::iri())
            << RDFStatement(imAffiliation, rdfs::label::iri(), LiteralValue("Other"))
            << RDFStatement(imAffiliation, nco::hasIMAddress::iri(), imAddress);
    inserts << RDFStatement(nco::default_contact_me::iri(), nco::hasAffiliation::iri(), imAffiliation)
            << RDFStatement(nco::default_contact_me::iri(), nco::contactUID::iri(), LiteralValue(strLocalUID))
            << RDFStatement(nco::default_contact_me::iri(), nco::contactLocalUID::iri(), LiteralValue(strLocalUID))
            << RDFStatement(nco::default_contact_me::iri(), nie::contentLastModified::iri(), LiteralValue(datetime));
    up.addDeletion(nco::default_contact_me::iri(), nie::contentLastModified::iri(), RDFVariable(), defaultGraph);

    up.addInsertion(inserts, defaultGraph);
    up.addInsertion(imAccountInserts, privateGraph);
    new CDTpUpdateQuery(up);
}

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper)
{
    CDTpStorageSyncOperations &op = mSyncOperations[accountWrapper];
    if (!op.active) {
        op.active = true;
        Q_EMIT syncStarted(accountWrapper);
    }

    syncAccountContacts(accountWrapper, accountWrapper->contacts(),
            QList<CDTpContactPtr>());

    /* We need to purge contacts that are in tracker but got removed from the
     * account while contactsd was not running.
     * We first query all contacts for that account, then will delete those that
     * are not in the account anymore. We can't use NOT IN() in the query
     * because with huge contact list it will hit SQL limit. */
    op.nPendingOperations++;
    CDTpAccountContactsSelectQuery *query = new CDTpAccountContactsSelectQuery(accountWrapper, this);
    connect(query,
            SIGNAL(finished(CDTpSelectQuery *)),
            SLOT(onContactPurgeSelectQueryFinished(CDTpSelectQuery *)));
}

void CDTpStorage::onContactPurgeSelectQueryFinished(CDTpSelectQuery *query)
{
    CDTpAccountContactsSelectQuery *contactsQuery =
        qobject_cast<CDTpAccountContactsSelectQuery*>(query);
    CDTpAccountPtr accountWrapper = contactsQuery->accountWrapper();

    LiveNodes result = query->reply();
    if (result->rowCount() <= 0) {
        oneSyncOperationFinished(accountWrapper);
        return;
    }

    RDFUpdate updateQuery;
    RDFStatementList deletions;
    RDFStatementList accountDeletions;
    RDFStatementList inserts;

    QList<QUrl> imAddressList;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, accountWrapper->contacts()) {
        imAddressList << contactImAddress(contactWrapper);
    }

    bool foundOne = false;
    CDTpStorageSyncOperations &op = mSyncOperations[accountWrapper];
    Q_FOREACH (const CDTpContactsSelectItem &item, contactsQuery->items()) {
        if (imAddressList.contains(item.imAddress)) {
            continue;
        }
        foundOne = true;
        op.nContactsRemoved++;
        addRemoveContactToQuery(updateQuery, inserts, deletions, item);
        addRemoveContactFromAccountToQuery(accountDeletions, item);
    }

    if (!foundOne) {
        oneSyncOperationFinished(accountWrapper);
        return;
    }

    updateQuery.addDeletion(deletions, defaultGraph);
    updateQuery.addInsertion(inserts, defaultGraph);
    updateQuery.addDeletion(accountDeletions, privateGraph);

    QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
    CDTpAccountsUpdateQuery *q = new CDTpAccountsUpdateQuery(accounts, updateQuery, this);
    connect(q,
            SIGNAL(finished(CDTpUpdateQuery *)),
            SLOT(onAccountsUpdateQueryFinished(CDTpUpdateQuery *)));
}

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contactsAdded,
        const QList<CDTpContactPtr> &contactsRemoved)
{
    Q_FOREACH (CDTpContactPtr contactWrapper, contactsAdded) {
        queueUpdate(contactWrapper, CDTpContact::All);
    }

    removeContacts(accountWrapper, contactsRemoved);
}

void CDTpStorage::syncAccountContact(CDTpAccountPtr accountWrapper,
        CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    Q_UNUSED(accountWrapper);
    queueUpdate(contactWrapper, changes);
}

void CDTpStorage::setAccountContactsOffline(CDTpAccountPtr accountWrapper)
{
    qDebug() << "Setting presence to UNKNOWN for all contacts of account"
             << accountWrapper->account()->objectPath();

    CDTpAccountContactsSelectQuery *query = new CDTpAccountContactsSelectQuery(accountWrapper, this);
    connect(query,
            SIGNAL(finished(CDTpSelectQuery *)),
            SLOT(onAccountOfflineSelectQueryFinished(CDTpSelectQuery *)));
}

void CDTpStorage::onAccountOfflineSelectQueryFinished(CDTpSelectQuery *query)
{
    RDFVariable unknownState = trackerStatusFromTpPresenceStatus(QLatin1String("unknown"));
    CDTpAccountContactsSelectQuery *contactsQuery =
        qobject_cast<CDTpAccountContactsSelectQuery*>(query);
    CDTpAccountPtr accountWrapper = contactsQuery->accountWrapper();

    RDFUpdate updateQuery;

    Q_FOREACH (const CDTpContactsSelectItem &item, contactsQuery->items()) {
        QUrl imContact(item.imContact);
        QUrl imAddress(item.imAddress);

        /* Update presence */
        updateQuery.addDeletion(imAddress, nco::imPresence::iri(),
            RDFVariable(), defaultGraph);
        updateQuery.addDeletion(imAddress, nco::presenceLastModified::iri(),
            RDFVariable(), defaultGraph);
        updateQuery.addDeletion(imContact, nie::contentLastModified::iri(),
            RDFVariable(), defaultGraph);

        updateQuery.addInsertion(imAddress, nco::imPresence::iri(),
            unknownState, defaultGraph);
        updateQuery.addInsertion(imAddress, nco::presenceLastModified::iri(),
            LiteralValue(QDateTime::currentDateTime()),defaultGraph);
        updateQuery.addInsertion(imContact, nie::contentLastModified::iri(),
            LiteralValue(QDateTime::currentDateTime()), defaultGraph);

        /* Update capabilities */
        RDFVariableList imAddressPropertyList;
        RDFStatementList inserts;
        addContactCapabilitiesInfoToQuery(inserts, imAddressPropertyList,
            imAddress, accountWrapper->account()->capabilities());
        Q_FOREACH (RDFVariable property, imAddressPropertyList) {
            updateQuery.addDeletion(imAddress, property, RDFVariable(), defaultGraph);
        }
        updateQuery.addInsertion(inserts, defaultGraph);
    }

    new CDTpUpdateQuery(updateQuery);
}

void CDTpStorage::removeAccount(const QString &accountObjectPath)
{
    qDebug() << "Removing account" << accountObjectPath << "from storage";

    /* Delete the imAccount resource */
    RDFUpdate updateQuery;
    const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
    updateQuery.addDeletion(imAccount, rdf::type::iri(), rdfs::Resource::iri(), privateGraph);
    new CDTpUpdateQuery(updateQuery);

    /* Delete all imAddress from that account */
    CDTpContactsSelectQuery *query = new CDTpContactsSelectQuery(accountObjectPath, this);
    connect(query,
            SIGNAL(finished(CDTpSelectQuery *)),
            SLOT(onAccountDeleteSelectQueryFinished(CDTpSelectQuery *)));
}

void CDTpStorage::onAccountDeleteSelectQueryFinished(CDTpSelectQuery *query)
{
    RDFUpdate updateQuery;
    RDFStatementList deletions;
    RDFStatementList inserts;

    CDTpContactsSelectQuery *contactsQuery = qobject_cast<CDTpContactsSelectQuery*>(query);
    Q_FOREACH (const CDTpContactsSelectItem &item, contactsQuery->items()) {
        addRemoveContactToQuery(updateQuery, inserts, deletions, item);
    }

    updateQuery.addDeletion(deletions, defaultGraph);
    updateQuery.addInsertion(inserts, defaultGraph);
    new CDTpUpdateQuery(updateQuery, this);
}

void CDTpStorage::removeContacts(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contacts)
{
    /* Split the request into smaller batches if necessary */
    if (contacts.size() > MAX_REMOVE_SIZE) {
        QList<CDTpContactPtr> batch;
        for (int i = 0; i < contacts.size(); i++) {
            batch << contacts[i];
            if (batch.size() == MAX_REMOVE_SIZE) {
                removeContacts(accountWrapper, batch);
                batch.clear();
            }
        }
        if (!batch.isEmpty()) {
            removeContacts(accountWrapper, batch);
            batch.clear();
        }
        return;
    }

    /* Cancel queued update if we are going to remove the contact anyway */
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        qDebug() << "Contact Removed, cancel update:" << contactWrapper->contact()->id();
        mUpdateQueue.remove(contactWrapper);
    }

    CDTpContactsSelectQuery *query = new CDTpContactsSelectQuery(contacts, this);
    connect(query,
            SIGNAL(finished(CDTpSelectQuery *)),
            SLOT(onContactDeleteSelectQueryFinished(CDTpSelectQuery *)));
}

void CDTpStorage::onContactDeleteSelectQueryFinished(CDTpSelectQuery *query)
{
    RDFUpdate updateQuery;
    RDFStatementList deletions;
    RDFStatementList accountDeletions;
    RDFStatementList inserts;

    CDTpContactsSelectQuery *contactsQuery = qobject_cast<CDTpContactsSelectQuery*>(query);
    Q_FOREACH (const CDTpContactsSelectItem &item, contactsQuery->items()) {
        addRemoveContactToQuery(updateQuery, inserts, deletions, item);
        addRemoveContactFromAccountToQuery(accountDeletions, item);
    }

    updateQuery.addDeletion(deletions, defaultGraph);
    updateQuery.addInsertion(inserts, defaultGraph);
    updateQuery.addDeletion(accountDeletions, privateGraph);
    new CDTpUpdateQuery(updateQuery, this);
}

void CDTpStorage::addRemoveContactToQuery(RDFUpdate &query,
        RDFStatementList &inserts,
        RDFStatementList &deletions,
        const CDTpContactsSelectItem &item)
{
    const QUrl imContact(item.imContact);
    const QUrl imAffiliation(item.imAffiliation);
    const QUrl imAddress(item.imAddress);
    bool deleteIMContact = (item.generator == defaultGenerator);

    qDebug() << "Deleting" << imAddress << "from" << imContact;
    qDebug() << "Also delete local contact:" << (deleteIMContact ? "Yes" : "No");

    /* Drop the imAddress and its affiliation */
    deletions << RDFStatement(imAffiliation, rdf::type::iri(), rdfs::Resource::iri());
    deletions << RDFStatement(imAddress, rdf::type::iri(), rdfs::Resource::iri());

    if (deleteIMContact) {
        /* The PersonContact is still owned by contactsd, drop it entirely */
        deletions << RDFStatement(imContact, rdf::type::iri(), rdfs::Resource::iri());
    } else {
        /* The PersonContact got modified by someone else, drop only the
         * hasAffiliation and keep the local contact in case it contains
         * additional info */
        deletions << RDFStatement(imContact, nco::hasAffiliation::iri(), imAffiliation);

        /* Update last modified time */
        const QDateTime datetime = QDateTime::currentDateTime();
        deletions << RDFStatement(imContact, nie::contentLastModified::iri(), RDFVariable());
        inserts << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime));
    }

    addRemoveContactInfoToQuery(query, imContact, imAddress);
}

void CDTpStorage::addRemoveContactFromAccountToQuery(RDFStatementList &deletions,
        const CDTpContactsSelectItem &item)
{
    const QUrl imAddress(item.imAddress);
    const QUrl imAccount(QUrl(item.imAddress.left(item.imAddress.indexOf("!"))));
    deletions << RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress);
}

void CDTpStorage::onContactUpdateSelectQueryFinished(CDTpSelectQuery *query)
{
    RDFUpdate updateQuery;
    RDFStatementList inserts;
    RDFStatementList imAccountInserts;
    RDFUpdate contactInfoQuery;
    QList<CDTpAccountPtr> accounts;

    CDTpContactResolver *resolver = qobject_cast<CDTpContactResolver*>(query);
    Q_FOREACH (CDTpContactPtr contactWrapper, resolver->remoteContacts()) {
        CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();

        /* Abort the update if the contact is not visible anymore. This could
         * happen if the contact got removed/blocked/etc while we were
         * resolving it. */
        if (!contactWrapper->isVisible()) {
            oneSyncOperationFinished(accountWrapper);
            continue;
        }

        /* Build a list of accounts for which contacts are being updated.
         * At this point nPendingOperations have one count per contact,
         * keep only one per account */
        if (!accounts.contains(accountWrapper)) {
            accounts << accountWrapper;
        } else {
            mSyncOperations[accountWrapper].nPendingOperations--;
        }

        QString localId = resolver->storageIdForContact(contactWrapper);
        bool resolved = !localId.isEmpty();
        if (!resolved) {
            localId = contactLocalId(contactWrapper);
            mSyncOperations[accountWrapper].nContactsAdded++;
        }

        const QString accountObjectPath = accountWrapper->account()->objectPath();
        const QString id = contactWrapper->contact()->id();

        qDebug() << "Updating" << id << "(" << localId << ")";

        RDFVariableList imAddressPropertyList;
        RDFVariableList imContactPropertyList;
        const RDFVariable imContact(contactIri(localId));
        const RDFVariable imAddress(contactImAddress(contactWrapper));
        const QDateTime datetime = QDateTime::currentDateTime();

        /* Create an imContact if we couldn't resolve to an existing one.
         * Otherwise just update its contentLastModified */
        if (!resolved) {
            inserts << RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri())
                    << RDFStatement(imAddress, nco::imID::iri(), LiteralValue(id))
                    << RDFStatement(imAddress, nco::imProtocol::iri(),
                                    LiteralValue(accountWrapper->account()->protocolName()));

            RDFVariable imAffiliation(contactAffiliation(contactWrapper));
            inserts << RDFStatement(imAffiliation, rdf::type::iri(), nco::Affiliation::iri())
                    << RDFStatement(imAffiliation, rdfs::label::iri(), LiteralValue("Other"))
                    << RDFStatement(imAffiliation, nco::hasIMAddress::iri(), imAddress);

            inserts << RDFStatement(imContact, rdf::type::iri(), nco::PersonContact::iri())
                    << RDFStatement(imContact, nco::contactLocalUID::iri(), LiteralValue(localId))
                    << RDFStatement(imContact, nco::contactUID::iri(), LiteralValue(localId))
                    << RDFStatement(imContact, nie::contentCreated::iri(), LiteralValue(datetime))
                    << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime))
                    << RDFStatement(imContact, nie::generator::iri(), LiteralValue(defaultGenerator))
                    << RDFStatement(imContact, nco::hasAffiliation::iri(), imAffiliation);
        } else {
            imContactPropertyList << nie::contentLastModified::iri();
            inserts << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime));
        }

        const CDTpContact::Changes changes = resolver->contactChanges(contactWrapper);
        if (changes & CDTpContact::Alias) {
            qDebug() << "  alias changed";
            addContactAliasInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Presence) {
            qDebug() << "  presence changed";
            addContactPresenceInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Capabilities) {
            qDebug() << "  capabilities changed";
            addContactCapabilitiesInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper->contact()->capabilities());
        }
        if (changes & CDTpContact::Avatar) {
            qDebug() << "  avatar changed";
            addContactAvatarInfoToQuery(updateQuery, inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Authorization) {
            qDebug() << "  authorization changed";
            addContactAuthorizationInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Infomation) {
            qDebug() << "  vcard information changed";
            /* Some ContactInfo insertions are made into per-contact graph. Also
             * some must be done *after* imContact insertions, so we add them
             * into another RDFUpdate, and we'll append it later. */
            addContactInfoToQuery(contactInfoQuery, inserts, imContact, contactWrapper);
        }

        // Link the IMAccount to this IMAddress
        const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
        imAccountInserts << RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress);

        Q_FOREACH (RDFVariable property, imContactPropertyList) {
            updateQuery.addDeletion(imContact, property, RDFVariable(), defaultGraph);
        }

        Q_FOREACH (RDFVariable property, imAddressPropertyList) {
            updateQuery.addDeletion(imAddress, property, RDFVariable(), defaultGraph);
        }
    }

    if (accounts.isEmpty()) {
        return;
    }

    updateQuery.addInsertion(inserts, defaultGraph);
    updateQuery.addInsertion(imAccountInserts, privateGraph);
    updateQuery.appendUpdate(contactInfoQuery);

    CDTpAccountsUpdateQuery *q = new CDTpAccountsUpdateQuery(accounts, updateQuery, this);
    connect(q,
            SIGNAL(finished(CDTpUpdateQuery *)),
            SLOT(onAccountsUpdateQueryFinished(CDTpUpdateQuery *)));
}

void CDTpStorage::saveAccountAvatar(RDFUpdate &query, const QByteArray &data, const QString &mimeType,
        const RDFVariable &imAddress, RDFStatementList &inserts)
{
    Q_UNUSED(mimeType);

    query.addDeletion(imAddress, nco::imAvatar::iri(), RDFVariable(), defaultGraph);

    if (data.isEmpty()) {
        return;
    }

    QString fileName = QString("%1/.contacts/avatars/%2")
        .arg(QDir::homePath())
        .arg(QString(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex()));
    qDebug() << "Saving account avatar to" << fileName;

    QFile avatarFile(fileName);
    if (!avatarFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to save account avatar: error opening avatar "
            "file" << fileName << "for writing";
        return;
    }
    avatarFile.write(data);
    avatarFile.close();

    RDFVariable dataObject(QUrl::fromLocalFile(fileName));
    query.addDeletion(dataObject, nie::url::iri(), RDFVariable(), defaultGraph);

    inserts << RDFStatement(dataObject, rdf::type::iri(), nie::DataObject::iri())
            << RDFStatement(dataObject, nie::url::iri(), dataObject)
            << RDFStatement(imAddress, nco::imAvatar::iri(), dataObject);
}

void CDTpStorage::addContactAliasInfoToQuery(RDFStatementList &inserts,
        RDFVariableList &properties,
        const RDFVariable &imAddress,
        CDTpContactPtr contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();
    properties << nco::imNickname::iri();
    inserts << RDFStatement(imAddress, nco::imNickname::iri(),
                LiteralValue(contact->alias()));
}

void CDTpStorage::addContactPresenceInfoToQuery(RDFStatementList &inserts,
        RDFVariableList &properties,
        const RDFVariable &imAddress,
        CDTpContactPtr contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    properties << nco::imPresence::iri() <<
        nco::imStatusMessage::iri() <<
        nco::presenceLastModified::iri();

    inserts << RDFStatement(imAddress, nco::imStatusMessage::iri(),
                LiteralValue(contact->presence().statusMessage())) <<
            RDFStatement(imAddress, nco::imPresence::iri(),
                trackerStatusFromTpPresenceType(contact->presence().type())) <<
            RDFStatement(imAddress, nco::presenceLastModified::iri(),
                LiteralValue(QDateTime::currentDateTime()));
}

void CDTpStorage::addContactCapabilitiesInfoToQuery(RDFStatementList &inserts,
        RDFVariableList &properties,
        const RDFVariable &imAddress,
        Tp::CapabilitiesBase capabilities)
{
    properties << nco::imCapability::iri();

    if (capabilities.textChats()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_text_chat::iri());
    }

    if (capabilities.streamedMediaAudioCalls()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_audio_calls::iri());
    }

    if (capabilities.streamedMediaVideoCalls()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_video_calls::iri());
    }
}

void CDTpStorage::addContactAvatarInfoToQuery(RDFUpdate &query,
        RDFStatementList &inserts,
        RDFVariableList &properties,
        const RDFVariable &imAddress,
        CDTpContactPtr contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    /* If we don't know the avatar token, it is preferable to keep the old
     * avatar until we get an update. */
    if (!contact->isAvatarTokenKnown()) {
        return;
    }

    /* If we have a token but not an avatar filename, that probably means the
     * avatar data is being requested and we'll get an update later. */
    if (!contact->avatarToken().isEmpty() &&
        contact->avatarData().fileName.isEmpty()) {
        return;
    }

    RDFVariable dataObject(QUrl::fromLocalFile(contact->avatarData().fileName));

    properties << nco::imAvatar::iri();
    query.addDeletion(dataObject, nie::url::iri(), RDFVariable(), defaultGraph);

    if (!contact->avatarToken().isEmpty()) {
        inserts << RDFStatement(dataObject, rdf::type::iri(), nie::DataObject::iri()) <<
            RDFStatement(dataObject, nie::url::iri(), dataObject) <<
            RDFStatement(imAddress, nco::imAvatar::iri(), dataObject);
    }
}

QUrl CDTpStorage::authStatus(Tp::Contact::PresenceState state)
{
    switch (state) {
    case Tp::Contact::PresenceStateNo:
        return nco::predefined_auth_status_no::iri();
    case Tp::Contact::PresenceStateAsk:
        return nco::predefined_auth_status_requested::iri();
    case Tp::Contact::PresenceStateYes:
        return nco::predefined_auth_status_yes::iri();
    }

    qWarning() << "Unknown telepathy presence state:" << state;
    return nco::predefined_auth_status_no::iri();
}

void CDTpStorage::addContactAuthorizationInfoToQuery(RDFStatementList &inserts,
        RDFVariableList &properties,
        const RDFVariable &imAddress,
        CDTpContactPtr contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    properties << nco::imAddressAuthStatusFrom::iri() <<
        nco::imAddressAuthStatusTo::iri();
    inserts << RDFStatement(imAddress, nco::imAddressAuthStatusFrom::iri(),
        RDFVariable(authStatus(contact->subscriptionState())));
    inserts << RDFStatement(imAddress, nco::imAddressAuthStatusTo::iri(),
        RDFVariable(authStatus(contact->publishState())));
}

void CDTpStorage::addRemoveContactInfoToQuery(RDFUpdate &query,
        const RDFVariable &imContact,
        const QUrl &graph)
{
    query.addDeletion(RDFVariable(), rdf::type::iri(), rdfs::Resource::iri(), graph);
    query.addDeletion(imContact, nco::hasAffiliation::iri(), RDFVariable(), graph);
    query.addDeletion(imContact, nco::birthDate::iri(), RDFVariable(), graph);
    query.addDeletion(imContact, nco::note::iri(), RDFVariable(), graph);
}

QString CDTpStorage::safeStringListAt(const QStringList &list, int i)
{
    if (i >= list.size()) {
        return QString();
    }

    return list.at(i);
}

void CDTpStorage::addContactInfoToQuery(RDFUpdate &query,
        RDFStatementList &inserts,
        const RDFVariable &imContact,
        CDTpContactPtr contactWrapper)
{
    /* Use the IMAddress URI as Graph URI for all its ContactInfo. This makes
     * easy to know which im contact those entities/properties belongs to */
    const QUrl graph = contactImAddress(contactWrapper);

    /* Drop current info */
    addRemoveContactInfoToQuery(query, imContact, graph);

    Tp::ContactPtr contact = contactWrapper->contact();
    Tp::ContactInfoFieldList listContactInfo = contact->infoFields().allFields();

    if (listContactInfo.count() == 0) {
        qDebug() << "No contact info present";
        return;
    }

    QHash<QString, RDFVariable> affiliationsMap;
    RDFStatementList graphInserts;

    Q_FOREACH (const Tp::ContactInfoField &field, listContactInfo) {
        if (field.fieldValue.count() == 0) {
            continue;
        }

        /* FIXME:
         *  - Do we care about "fn" and "nickname" ?
         *  - How do we write affiliation for "org" ?
         */
        if (!field.fieldName.compare("tel")) {
            addContactVoicePhoneNumberToQuery(graphInserts, inserts,
                    ensureAffiliation(affiliationsMap, graphInserts, imContact, field),
                    safeStringListAt(field.fieldValue, 0));
        } else if (!field.fieldName.compare("adr")) {
            addContactAddressToQuery(graphInserts,
                    ensureAffiliation(affiliationsMap, graphInserts, imContact, field),
                    safeStringListAt(field.fieldValue, 0),
                    safeStringListAt(field.fieldValue, 1),
                    safeStringListAt(field.fieldValue, 2),
                    safeStringListAt(field.fieldValue, 3),
                    safeStringListAt(field.fieldValue, 4),
                    safeStringListAt(field.fieldValue, 5),
                    safeStringListAt(field.fieldValue, 6));
        } else if (!field.fieldName.compare("email")) {
            addContactEmailToQuery(graphInserts, inserts,
                    ensureAffiliation(affiliationsMap, graphInserts, imContact, field),
                    safeStringListAt(field.fieldValue, 0));
        } else if (!field.fieldName.compare("url")) {
            RDFVariable affiliation = ensureAffiliation(affiliationsMap, graphInserts, imContact, field);
            graphInserts << RDFStatement(affiliation, nco::url::iri(), LiteralValue(safeStringListAt(field.fieldValue, 0)));
        } else if (!field.fieldName.compare("title")) {
            RDFVariable affiliation = ensureAffiliation(affiliationsMap, graphInserts, imContact, field);
            graphInserts << RDFStatement(affiliation, nco::title::iri(), LiteralValue(safeStringListAt(field.fieldValue, 0)));
        } else if (!field.fieldName.compare("role")) {
            RDFVariable affiliation = ensureAffiliation(affiliationsMap, graphInserts, imContact, field);
            graphInserts << RDFStatement(affiliation, nco::role::iri(), LiteralValue(safeStringListAt(field.fieldValue, 0)));
        } else if (!field.fieldName.compare("note") || !field.fieldName.compare("desc")) {
            graphInserts << RDFStatement(imContact, nco::note::iri(), LiteralValue(safeStringListAt(field.fieldValue, 0)));
        } else if (!field.fieldName.compare("bday")) {
            /* Tracker will reject anything not [-]CCYY-MM-DDThh:mm:ss[Z|(+|-)hh:mm]
             * VCard spec allows only ISO 8601, but most IM clients allows
             * any string. */
            /* FIXME: support more date format for compatibility */
            QDate date = QDate::fromString(safeStringListAt(field.fieldValue, 0), "yyyy-MM-dd");
            if (!date.isValid()) {
                date = QDate::fromString(safeStringListAt(field.fieldValue, 0), "yyyyMMdd");
            }

            if (date.isValid()) {
                graphInserts << RDFStatement(imContact, nco::birthDate::iri(), LiteralValue(date));
            } else {
                qDebug() << "Unsupported bday format:" << safeStringListAt(field.fieldValue, 0);
            }
        } else {
            qDebug() << "Unsupported VCard field" << field.fieldName;
        }
    }

    query.addInsertion(graphInserts, graph);
}

RDFVariable CDTpStorage::ensureAffiliation(QHash<QString, RDFVariable> &map,
        RDFStatementList &graphInserts,
        const RDFVariable &imContact,
        const Tp::ContactInfoField &field)
{
    /* FIXME: Do we support more types? */
    static QHash<QString, QString> knownTypes;
    if (knownTypes.isEmpty()) {
        knownTypes.insert ("work", "Work");
        knownTypes.insert ("home", "Home");
    }

    QString type = "Other";
    Q_FOREACH (const QString &parameter, field.parameters) {
        if (!parameter.startsWith("type=")) {
            continue;
        }

        const QString str = parameter.mid(5);
        if (knownTypes.contains(str)) {
            type = knownTypes[str];
            break;
        }
    }

    if (!map.contains(type)) {
        static uint counter = 0;
        RDFVariable affiliation = RDFVariable(QString("affiliation%1").arg(++counter));
        graphInserts << RDFStatement(affiliation, rdf::type::iri(), nco::Affiliation::iri())
                     << RDFStatement(affiliation, rdfs::label::iri(), LiteralValue(type))
                     << RDFStatement(imContact, nco::hasAffiliation::iri(), affiliation);
        map.insert(type, affiliation);
    }

    return map[type];
}

void CDTpStorage::addContactVoicePhoneNumberToQuery(RDFStatementList &graphInserts,
        RDFStatementList &inserts,
        const RDFVariable &affiliation,
        const QString &phoneNumber)
{
    RDFVariable voicePhoneNumber = QUrl(QString("tel:%1").arg(phoneNumber));
    inserts << RDFStatement(voicePhoneNumber, rdf::type::iri(), nco::VoicePhoneNumber::iri())
            << RDFStatement(voicePhoneNumber, maemo::localPhoneNumber::iri(), LiteralValue(phoneNumber))
            << RDFStatement(voicePhoneNumber, nco::phoneNumber::iri(), LiteralValue(phoneNumber));

    graphInserts << RDFStatement(affiliation, nco::hasPhoneNumber::iri(), voicePhoneNumber);
}

void CDTpStorage::addContactAddressToQuery(RDFStatementList &graphInserts,
        const RDFVariable &affiliation,
        const QString &pobox,
        const QString &extendedAddress,
        const QString &streetAddress,
        const QString &locality,
        const QString &region,
        const QString &postalcode,
        const QString &country)
{
    static uint counter = 0;
    RDFVariable imPostalAddress = RDFVariable(QString("address%1").arg(++counter));
    graphInserts << RDFStatement(imPostalAddress, rdf::type::iri(), nco::PostalAddress::iri())
                 << RDFStatement(imPostalAddress, nco::pobox::iri(), LiteralValue(pobox))
                 << RDFStatement(imPostalAddress, nco::extendedAddress::iri(), LiteralValue(extendedAddress))
                 << RDFStatement(imPostalAddress, nco::streetAddress::iri(), LiteralValue(streetAddress))
                 << RDFStatement(imPostalAddress, nco::locality::iri(), LiteralValue(locality))
                 << RDFStatement(imPostalAddress, nco::region::iri(), LiteralValue(region))
                 << RDFStatement(imPostalAddress, nco::postalcode::iri(), LiteralValue(postalcode))
                 << RDFStatement(imPostalAddress, nco::country::iri(), LiteralValue(country));

    graphInserts << RDFStatement(affiliation, nco::hasPostalAddress::iri(), imPostalAddress);
}

void CDTpStorage::addContactEmailToQuery(RDFStatementList &graphInserts,
        RDFStatementList &inserts,
        const RDFVariable &affiliation,
        const QString &email)
{
    RDFVariable emailAddress = QUrl(QString("mailto:%1").arg(email));
    inserts << RDFStatement(emailAddress, rdf::type::iri(), nco::EmailAddress::iri())
            << RDFStatement(emailAddress, nco::emailAddress::iri(), LiteralValue(email));

    graphInserts << RDFStatement(affiliation, nco::hasEmailAddress::iri(), emailAddress);
}

QString CDTpStorage::contactLocalId(const QString &contactAccountObjectPath,
        const QString &contactId)
{
    return QString::number(qHash(QString("%1!%2")
                .arg(contactAccountObjectPath)
                .arg(contactId)));
}

QString CDTpStorage::contactLocalId(CDTpContactPtr contactWrapper)
{
    CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();
    return contactLocalId(account->objectPath(), contact->id());
}

QUrl CDTpStorage::contactIri(const QString &contactLocalId)
{
    return QUrl(QString("contact:%1").arg(contactLocalId));
}

QUrl CDTpStorage::contactIri(CDTpContactPtr contactWrapper)
{
    return contactIri(contactLocalId(contactWrapper));
}

QUrl CDTpStorage::contactImAddress(const QString &contactAccountObjectPath,
        const QString &contactId)
{
    return QUrl(QString("telepathy:%1!%2")
            .arg(contactAccountObjectPath)
            .arg(contactId));
}

QUrl CDTpStorage::contactImAddress(CDTpContactPtr contactWrapper)
{
    CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();
    return contactImAddress(account->objectPath(), contact->id());
}

QUrl CDTpStorage::contactAffiliation(const QString &contactAccountObjectPath,
        const QString &contactId)
{
    return QUrl(QString("affiliationtelepathy:%1!%2")
            .arg(contactAccountObjectPath)
            .arg(contactId));
}

QUrl CDTpStorage::contactAffiliation(CDTpContactPtr contactWrapper)
{
    CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();
    return contactAffiliation(account->objectPath(), contact->id());
}

QUrl CDTpStorage::trackerStatusFromTpPresenceType(uint tpPresenceType)
{
    switch (tpPresenceType) {
    case Tp::ConnectionPresenceTypeUnset:
        return nco::presence_status_unknown::iri();
    case Tp::ConnectionPresenceTypeOffline:
        return nco::presence_status_offline::iri();
    case Tp::ConnectionPresenceTypeAvailable:
        return nco::presence_status_available::iri();
    case Tp::ConnectionPresenceTypeAway:
        return nco::presence_status_away::iri();
    case Tp::ConnectionPresenceTypeExtendedAway:
        return nco::presence_status_extended_away::iri();
    case Tp::ConnectionPresenceTypeHidden:
        return nco::presence_status_hidden::iri();
    case Tp::ConnectionPresenceTypeBusy:
        return nco::presence_status_busy::iri();
    case Tp::ConnectionPresenceTypeUnknown:
        return nco::presence_status_unknown::iri();
    case Tp::ConnectionPresenceTypeError:
        return nco::presence_status_error::iri();
    default:
        qWarning() << "Unknown telepathy presence status" << tpPresenceType;
    }

    return nco::presence_status_error::iri();
}

QUrl CDTpStorage::trackerStatusFromTpPresenceStatus(
        const QString &tpPresenceStatus)
{
    static QHash<QString, QUrl> mapping;
    if (mapping.isEmpty()) {
        mapping.insert("offline", nco::presence_status_offline::iri());
        mapping.insert("available", nco::presence_status_available::iri());
        mapping.insert("away", nco::presence_status_away::iri());
        mapping.insert("xa", nco::presence_status_extended_away::iri());
        mapping.insert("dnd", nco::presence_status_busy::iri());
        mapping.insert("busy", nco::presence_status_busy::iri());
        mapping.insert("hidden", nco::presence_status_hidden::iri());
        mapping.insert("unknown", nco::presence_status_unknown::iri());
    }

    QHash<QString, QUrl>::const_iterator i(mapping.constFind(tpPresenceStatus));
    if (i != mapping.end()) {
        return *i;
    }
    return nco::presence_status_error::iri();
}

void CDTpStorage::queueUpdate(CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    if (!mUpdateQueue.contains(contactWrapper)) {
        qDebug() << "queue update for" << contactWrapper->contact()->id();
        mUpdateQueue.insert(contactWrapper, changes);
        mSyncOperations[contactWrapper->accountWrapper()].nPendingOperations++;
    } else {
        mUpdateQueue[contactWrapper] |= changes;
    }

    /* If the queue is too big, flush it now to avoid hitting query size limit */
    if (mUpdateQueue.size() >= MAX_UPDATE_SIZE) {
        onQueueTimerTimeout();
    } else if (!mQueueTimer.isActive()) {
        mQueueTimer.start();
    }
}

void CDTpStorage::onQueueTimerTimeout()
{
    if (mUpdateQueue.isEmpty()) {
        return;
    }

    CDTpContactResolver *query = new CDTpContactResolver(mUpdateQueue, this);
    connect(query,
            SIGNAL(finished(CDTpSelectQuery *)),
            SLOT(onContactUpdateSelectQueryFinished(CDTpSelectQuery *)));

    mUpdateQueue.clear();
}

void CDTpStorage::executeQuery(const QSparqlQuery &query)
{
    QSparqlConnection &connection = com::nokia::contactsd::SparqlConnectionManager::defaultConnection();
    QSparqlResult *result = connection.exec(query);

    if (not result) {
        qWarning() << Q_FUNC_INFO << " - QSparqlConnection::exec() == 0";
        return;
    }
    if (result->hasError()) {
        qDebug() << Q_FUNC_INFO << result->lastError().message();
        delete result;
        return;
    }

    result->setParent(this);
    connect(result, SIGNAL(finished()), SLOT(onQueryFinished()), Qt::QueuedConnection);
}

void CDTpStorage::onQueryFinished()
{
    QSparqlResult *const result = qobject_cast<QSparqlResult *>(sender());

    if (not result) {
        qWarning() << Q_FUNC_INFO << ("Ignoring signal from invalid sender.");
        return;
    }

    if (result->hasError()) {
        qDebug() << Q_FUNC_INFO << result->lastError().message();
    }
    result->deleteLater();
}

void CDTpStorage::oneSyncOperationFinished(CDTpAccountPtr accountWrapper)
{
    CDTpStorageSyncOperations &op = mSyncOperations[accountWrapper];
    op.nPendingOperations--;

    if (op.nPendingOperations == 0) {
        if (op.active) {
            Q_EMIT syncEnded(accountWrapper, op.nContactsAdded, op.nContactsRemoved);
        }
        mSyncOperations.remove(accountWrapper);
    }
}

void CDTpStorage::onAccountsUpdateQueryFinished(CDTpUpdateQuery *query)
{
    CDTpAccountsUpdateQuery *accountsQuery = qobject_cast<CDTpAccountsUpdateQuery *>(query);

    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accountsQuery->accounts()) {
        oneSyncOperationFinished(accountWrapper);
    }
}

CDTpStorageSyncOperations::CDTpStorageSyncOperations() : active(false),
    nPendingOperations(0), nContactsAdded(0), nContactsRemoved(0)
{
}

