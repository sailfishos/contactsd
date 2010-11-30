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

#include "cdtpstorage.h"

#include <TelepathyQt4/AvatarData>
#include <TelepathyQt4/ContactCapabilities>

const QUrl CDTpStorage::defaultGraph = QUrl(
        QLatin1String("urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9"));
const QUrl CDTpStorage::privateGraph = QUrl(
        QLatin1String("urn:uuid:679293d4-60f0-49c7-8d63-f1528fe31f66"));

const QString defaultGenerator = "telepathy";

CDTpStorage::CDTpStorage(QObject *parent)
    : QObject(parent)
{
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

    CDTpStorageSelectQuery *query = new CDTpStorageSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpStorageSelectQuery *)),
            SLOT(onAccountPurgeSelectQueryFinished(CDTpStorageSelectQuery *)));
}

void CDTpStorage::onAccountPurgeSelectQueryFinished(CDTpStorageSelectQuery *query)
{
    LiveNodes result = query->reply();

    for (int i = 0; i < result->rowCount(); i++) {
        const QString accountUrl = result->index(i, 0).data().toString();
        const QString accountObjectPath = accountUrl.mid(QString("telepathy:").length());

        removeAccount(accountObjectPath);
    }

    query->deleteLater();
}

void CDTpStorage::syncAccount(CDTpAccount *accountWrapper)
{
    syncAccount(accountWrapper, CDTpAccount::All);
}

// TODO: Improve syncAccount so that it only updates the data that really
//       changed
void CDTpStorage::syncAccount(CDTpAccount *accountWrapper,
        CDTpAccount::Changes changes)
{
    Tp::AccountPtr account = accountWrapper->account();
    if (account->normalizedName().isEmpty()) {
        return;
    }

    const QString accountObjectPath = account->objectPath();
    const QString accountId = account->normalizedName();

    qDebug() << "Syncing account" << accountObjectPath << "to storage" << accountId;

    RDFUpdate up;
    RDFStatementList inserts;

    // Create the IMAddress for this account's self contact
    RDFVariable imAddress(contactImAddress(accountObjectPath, accountId));
    up.addDeletion(imAddress, nco::imID::iri(), RDFVariable(), defaultGraph);
    inserts << RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri())
            << RDFStatement(imAddress, nco::imID::iri(), LiteralValue(account->normalizedName()));

    if (changes & CDTpAccount::Nickname) {
       up.addDeletion(imAddress, nco::imNickname::iri(), RDFVariable(), defaultGraph);
       inserts << RDFStatement(imAddress, nco::imNickname::iri(), LiteralValue(account->nickname()));
    }

    if (changes & CDTpAccount::Presence) {
        Tp::Presence presence = account->currentPresence();
        QUrl status = trackerStatusFromTpPresenceStatus(presence.status());

        up.addDeletion(imAddress, nco::imPresence::iri(), RDFVariable(), defaultGraph);
        up.addDeletion(imAddress, nco::imStatusMessage::iri(), RDFVariable(), defaultGraph);
        up.addDeletion(imAddress, nco::presenceLastModified::iri(), RDFVariable(), defaultGraph);

        inserts << RDFStatement(imAddress, nco::imPresence::iri(), RDFVariable(status))
                << RDFStatement(imAddress, nco::imStatusMessage::iri(), LiteralValue(presence.statusMessage()))
                << RDFStatement(imAddress, nco::presenceLastModified::iri(), LiteralValue(QDateTime::currentDateTime()));
    }

    if (changes & CDTpAccount::Avatar) {
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
                     << RDFStatement(imAccount, nco::imProtocol::iri(), LiteralValue(account->protocolName()))
                     << RDFStatement(imAccount, nco::imAccountAddress::iri(), imAddress)
                     << RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress);

    if (changes & CDTpAccount::DisplayName) {
        up.addDeletion(imAccount, nco::imDisplayName::iri(), RDFVariable(), privateGraph);
        imAccountInserts << RDFStatement(imAccount, nco::imDisplayName::iri(), LiteralValue(account->displayName()));
    }

    // link the IMAddress to me-contact
    const QString strLocalUID = QString::number(0x7FFFFFFF);
    inserts << RDFStatement(nco::default_contact_me::iri(), nco::hasIMAddress::iri(), imAddress)
            << RDFStatement(nco::default_contact_me::iri(), nco::contactUID::iri(), LiteralValue(strLocalUID))
            << RDFStatement(nco::default_contact_me::iri(), nco::contactLocalUID::iri(), LiteralValue(strLocalUID));

    up.addInsertion(inserts, defaultGraph);
    up.addInsertion(imAccountInserts, privateGraph);
    ::tracker()->executeQuery(up);
}

void CDTpStorage::syncAccountContacts(CDTpAccount *accountWrapper)
{
    // TODO: return the number of contacts that were actually added
    syncAccountContacts(accountWrapper, accountWrapper->contacts(),
            QList<CDTpContactPtr>());

    /* We need to purge contacts that are in tracker but got removed from the
     * account while contactsd was not running. */
    removeContacts(accountWrapper, accountWrapper->contacts(), true);
}

void CDTpStorage::syncAccountContacts(CDTpAccount *accountWrapper,
        const QList<CDTpContactPtr> &contactsAdded,
        const QList<CDTpContactPtr> &contactsRemoved)
{
    Tp::AccountPtr account = accountWrapper->account();
    QString accountObjectPath = account->objectPath();

    /* Don't add contacts that are blocked */
    QList<CDTpContactPtr> realAdded;
    Q_FOREACH (CDTpContactPtr contactWrapper, contactsAdded) {
        Tp::ContactPtr contact = contactWrapper->contact();
        if (!contact->isBlocked()) {
            realAdded << contactWrapper;
        }
    }

    qDebug() << "Syncing account" << accountObjectPath <<
        "roster contacts to storage";
    qDebug() << " " << realAdded.size() << "contacts added";
    qDebug() << " " << contactsRemoved.size() << "contacts removed";

    CDTpStorageContactResolver *addResolver =
        new CDTpStorageContactResolver(accountWrapper, realAdded, this);
    connect(addResolver,
            SIGNAL(finished(CDTpStorageContactResolver *)),
            SLOT(onContactAddResolverFinished(CDTpStorageContactResolver *)));

    removeContacts(accountWrapper, contactsRemoved);
}

void CDTpStorage::syncAccountContact(CDTpAccount *accountWrapper,
        CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();

    if (changes & CDTpContact::Blocked) {
        qDebug() << "Contact Block Status changed";

        if (contact->isBlocked()) {
            removeContacts(accountWrapper, QList<CDTpContactPtr>() << contactWrapper);
        } else {
            syncAccountContacts(accountWrapper,
                    QList<CDTpContactPtr>() << contactWrapper,
                    QList<CDTpContactPtr>());
        }

        return;
    }

    CDTpStorageContactResolver *updateResolver =
        new CDTpStorageContactResolver(accountWrapper,
                QList<CDTpContactPtr>() << contactWrapper, this);
    updateResolver->setContactChanges(changes);
    connect(updateResolver,
            SIGNAL(finished(CDTpStorageContactResolver *)),
            SLOT(onContactUpdateResolverFinished(CDTpStorageContactResolver *)));
}

void CDTpStorage::setAccountContactsOffline(CDTpAccount *accountWrapper)
{
    Tp::AccountPtr account = accountWrapper->account();

    qDebug() << "Setting account" << account->objectPath() <<
        "contacts presence to Offline on storage";

    RDFSelect select;
    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAddress = imContact.property<nco::hasIMAddress>();
    imAddress.hasPrefix(QString("telepathy:%1").arg(account->objectPath()));

    select.addColumn("contact", imContact);
    select.addColumn("imAddress", imAddress);

    CDTpStorageSelectQuery *query = new CDTpStorageSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpStorageSelectQuery *)),
            SLOT(onAccountOfflineSelectQueryFinished(CDTpStorageSelectQuery *)));
}

void CDTpStorage::removeAccount(const QString &accountObjectPath)
{
    qDebug() << "Removing account" << accountObjectPath << "from storage";

    /* Delete the imAccount resource */
    RDFUpdate updateQuery;
    const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
    updateQuery.addDeletion(imAccount, rdf::type::iri(), rdfs::Resource::iri(), privateGraph);
    ::tracker()->executeQuery(updateQuery);

    /* Delete all imAddress from that account */
    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAddress = imContact.property<nco::hasIMAddress>();
    imAddress.hasPrefix(QString("telepathy:%1").arg(accountObjectPath));

    RDFSelect select;
    select.addColumn("contact", imContact);
    select.addColumn("address", imAddress);
    select.addColumn("Generator", imContact.property<nie::generator>());

    CDTpStorageSelectQuery *query = new CDTpStorageSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpStorageSelectQuery *)),
            SLOT(onAccountDeleteSelectQueryFinished(CDTpStorageSelectQuery *)));
}

void CDTpStorage::onAccountDeleteSelectQueryFinished(CDTpStorageSelectQuery *query)
{
    removeContacts(query, false);
}

void CDTpStorage::removeContacts(CDTpAccount *accountWrapper,
        const QList<CDTpContactPtr> &contacts, bool not_)
{
    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAddress = imContact.property<nco::hasIMAddress>();
    RDFVariableList members;
    Q_FOREACH (CDTpContactPtr contactWrapper, contacts) {
        members << RDFVariable(contactImAddress(contactWrapper));
    }

    if (not_) {
        /* We want to remove all contacts from that account, EXCEPT those in
         * contacts param. */
        const QString accountId = accountWrapper->account()->normalizedName();
        const QString accountPath = accountWrapper->account()->objectPath();

        imAddress.isMemberOf(members).not_();
        imAddress.hasPrefix(QString("telepathy:%1").arg(accountPath));
        imAddress.notEqual(contactImAddress(accountPath, accountId));
    } else {
        imAddress.isMemberOf(members);
    }

    RDFSelect select;
    select.addColumn("Contact", imContact);
    select.addColumn("Address", imAddress);
    select.addColumn("Generator", imContact.property<nie::generator>());

    CDTpStorageSelectQuery *query = new CDTpStorageSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpStorageSelectQuery *)),
            SLOT(onContactDeleteSelectQueryFinished(CDTpStorageSelectQuery *)));
}

void CDTpStorage::onContactDeleteSelectQueryFinished(CDTpStorageSelectQuery *query)
{
    removeContacts(query, true);
}

void CDTpStorage::removeContacts(CDTpStorageSelectQuery *query, bool deleteAccount)
{
    query->deleteLater();

    LiveNodes result = query->reply();
    if (result->rowCount() <= 0) {
        return;
    }

    RDFUpdate updateQuery;
    for (int i = 0; i < result->rowCount(); i++) {
        QUrl imContact = QUrl(result->index(i, 0).data().toString());
        QUrl imAddress = QUrl(result->index(i, 1).data().toString());
        const QString generator = result->index(i, 2).data().toString();

        if (deleteAccount) {
            const QString addressUrl = result->index(i, 1).data().toString();
            const RDFVariable imAccount(QUrl(addressUrl.left(addressUrl.indexOf("!"))));
            updateQuery.addDeletion(imAccount, nco::hasIMContact::iri(), imAddress, privateGraph);
        }

        updateQuery.addDeletion(imAddress, rdf::type::iri(), rdfs::Resource::iri(), defaultGraph);

        if (generator == defaultGenerator) {
            updateQuery.addDeletion(imContact, rdf::type::iri(), rdfs::Resource::iri(), defaultGraph);
        } else {
            updateQuery.addDeletion(imContact, nco::hasIMAddress::iri(),
                    imAddress, defaultGraph);
            updateQuery.addDeletion(imContact, nie::contentLastModified::iri(),
                    RDFVariable(), defaultGraph);
            updateQuery.addInsertion(imContact, nie::contentLastModified::iri(),
                    LiteralValue(QDateTime::currentDateTime()), defaultGraph);
        }
    }

    ::tracker()->executeQuery(updateQuery);
}

void CDTpStorage::onAccountOfflineSelectQueryFinished(
        CDTpStorageSelectQuery * query)
{
    LiveNodes contactNodes = query->reply();
    RDFVariable unknownState =
        trackerStatusFromTpPresenceStatus(QLatin1String("unknown"));
    RDFUpdate update;
    for (int i = 0; i < contactNodes->rowCount(); ++i) {
        QUrl imContactIri =
            QUrl(contactNodes->index(i, 0).data().toString());
        QUrl imAddressIri =
            QUrl(contactNodes->index(i, 1).data().toString());
        RDFVariable imContact = QUrl(imContactIri);
        RDFVariable imAddress = QUrl(imAddressIri);

        update.addDeletion(imAddress, nco::imPresence::iri(),
                RDFVariable(), defaultGraph);
        update.addDeletion(imAddress, nco::presenceLastModified::iri(),
                RDFVariable(), defaultGraph);
        update.addDeletion(imContact, nie::contentLastModified::iri(),
                RDFVariable(), defaultGraph);

        update.addInsertion(imAddress, nco::imPresence::iri(),
                unknownState, defaultGraph);
        update.addInsertion(imAddress, nco::presenceLastModified::iri(),
                LiteralValue(QDateTime::currentDateTime()),defaultGraph);
        update.addInsertion(imContact, nie::contentLastModified::iri(),
                LiteralValue(QDateTime::currentDateTime()), defaultGraph);
    }

    ::tracker()->executeQuery(update);
}

void CDTpStorage::onContactAddResolverFinished(CDTpStorageContactResolver *resolver)
{
    RDFUpdate updateQuery;
    RDFStatementList inserts;
    RDFStatementList imAccountInserts;
    QList<RDFUpdate> updateQueryQueue;

    Q_FOREACH (CDTpContactPtr contactWrapper, resolver->remoteContacts()) {
        Tp::ContactPtr contact = contactWrapper->contact();
        QString accountObjectPath =
            contactWrapper->accountWrapper()->account()->objectPath();

        const QString id = contact->id();
        QString localId = resolver->storageIdForContact(contactWrapper);
        bool alreadyExists = !localId.isEmpty();

        if (!alreadyExists) {
            localId = contactLocalId(accountObjectPath, id);
        }

        RDFVariableList imAddressPropertyList;
        RDFVariableList imContactPropertyList;
        const RDFVariable imContact(contactIri(localId));
        const RDFVariable imAddress(contactImAddress(accountObjectPath, id));
        const QDateTime datetime = QDateTime::currentDateTime();

        /* Insert the imContact only if we didn't found one. UI could already
         * have created the imContact before adding the im contact in telepathy
         */
        if (!alreadyExists) {
            inserts << RDFStatement(imContact, rdf::type::iri(), nco::PersonContact::iri())
                    << RDFStatement(imContact, nco::contactLocalUID::iri(), LiteralValue(localId))
                    << RDFStatement(imContact, nco::contactUID::iri(), LiteralValue(localId))
                    << RDFStatement(imContact, nie::contentCreated::iri(), LiteralValue(datetime))
                    << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime))
                    << RDFStatement(imContact, nie::generator::iri(), LiteralValue(defaultGenerator));
        } else {
            imContactPropertyList << nie::contentLastModified::iri();
            inserts << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime));
        }

        // Insert the IMAddress
        inserts << RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri())
                << RDFStatement(imAddress, nco::imID::iri(), LiteralValue(id));

        addContactAliasInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        addContactPresenceInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        addContactCapabilitiesInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        addContactAvatarInfoToQuery(updateQuery, inserts, imAddressPropertyList,
                imAddress, contactWrapper);
        addContactAuthorizationInfoToQuery(inserts, imAddressPropertyList,
                imAddress, contactWrapper);

        /* ContactInfo insertions are made into per-contact graph, so we can't
         * use 'inserts' here. We also have to make sure those insertions are
         * made *after* imContact insertions, so we queue them into a list,
         * and we'll append them later. */
        RDFUpdate query;
        addContactInfoToQuery(query, imContact, contactWrapper);
        updateQueryQueue << query;

        // Link the IMAccount to this IMAddress
        const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
        imAccountInserts << RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress);

        // Link the IMContact to this IMAddress
        inserts << RDFStatement(imContact, nco::hasIMAddress::iri(), imAddress);

        Q_FOREACH (RDFVariable property, imContactPropertyList) {
            updateQuery.addDeletion(imContact, property, RDFVariable(), defaultGraph);
        }

        Q_FOREACH (RDFVariable property, imAddressPropertyList) {
            updateQuery.addDeletion(imAddress, property, RDFVariable(), defaultGraph);
        }
    }

    updateQuery.addInsertion(inserts, defaultGraph);
    updateQuery.addInsertion(imAccountInserts, privateGraph);
    Q_FOREACH (const RDFUpdate &query, updateQueryQueue) {
        updateQuery.appendUpdate(query);
    }

    ::tracker()->executeQuery(updateQuery);
    resolver->deleteLater();
}

void CDTpStorage::onContactUpdateResolverFinished(CDTpStorageContactResolver *resolver)
{
    RDFUpdate updateQuery;
    RDFStatementList inserts;

    Q_FOREACH (CDTpContactPtr contactWrapper, resolver->remoteContacts()) {
        if (contactWrapper->isRemoved()) {
            continue;
        }

        Tp::ContactPtr contact = contactWrapper->contact();
        QString accountObjectPath =
            contactWrapper->accountWrapper()->account()->objectPath();

        const QString id = contact->id();
        QString localId = resolver->storageIdForContact(contactWrapper);

        if (localId.isEmpty() || localId.isNull()) {
            localId = contactLocalId(accountObjectPath, id);
        }
        qDebug() << Q_FUNC_INFO << "Updating " << localId;

        RDFVariableList imAddressPropertyList;
        RDFVariableList imContactPropertyList;
        const RDFVariable imContact(contactIri(localId));
        const RDFVariable imAddress(contactImAddress(accountObjectPath, id));
        const QDateTime datetime = QDateTime::currentDateTime();

        imContactPropertyList << nie::contentLastModified::iri();
        inserts << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime));

        const CDTpContact::Changes changes = resolver->contactChanges();
        if (changes & CDTpContact::Alias) {
            qDebug() << "  alias changed";
            addContactAliasInfoToQuery(inserts,
                   imAddressPropertyList, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Presence) {
            qDebug() << "  presence changed";
            addContactPresenceInfoToQuery(inserts,
                    imAddressPropertyList, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Capabilities) {
            qDebug() << "  capabilities changed";
            addContactCapabilitiesInfoToQuery(inserts,
                    imAddressPropertyList, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Avatar) {
            qDebug() << "  avatar changed";
           addContactAvatarInfoToQuery(updateQuery, inserts,
                   imAddressPropertyList, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Authorization) {
            qDebug() << "  authorization changed";
            addContactAuthorizationInfoToQuery(inserts,
                    imAddressPropertyList, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Infomation) {
            qDebug() << "  vcard information changed";
            addContactInfoToQuery(updateQuery, imContact, contactWrapper);
        }

        Q_FOREACH (RDFVariable property, imContactPropertyList) {
            updateQuery.addDeletion(imContact, property, RDFVariable(), defaultGraph);
        }

        Q_FOREACH (RDFVariable property, imAddressPropertyList) {
            updateQuery.addDeletion(imAddress, property, RDFVariable(), defaultGraph);
        }
    }

    updateQuery.addInsertion(inserts, defaultGraph);
    ::tracker()->executeQuery(updateQuery);
    resolver->deleteLater();
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
        CDTpContactPtr contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    properties << nco::imCapability::iri();

    if (contact->capabilities().textChats()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_text_chat::iri());
    }

    if (contact->capabilities().streamedMediaAudioCalls()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_audio_calls::iri());
    }

    if (contact->capabilities().streamedMediaVideoCalls()) {
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

void CDTpStorage::addContactInfoToQuery(RDFUpdate &query,
        const RDFVariable &imContact,
        CDTpContactPtr contactWrapper)
{
    /* Use the IMAddress URI as Graph URI for all its ContactInfo. This makes
     * easy to know which im contact those entities/properties belongs to */
    const QUrl graph = contactImAddress(contactWrapper);

    /* Drop everything from this graph */
    query.addDeletion(RDFVariable(), rdf::type::iri(), rdfs::Resource::iri(), graph);
    query.addDeletion(imContact, nco::hasAffiliation::iri(), RDFVariable(), graph);

    /* FIXME: Tp::Contact::info() is deprecated */
    Tp::ContactPtr contact = contactWrapper->contact();
    Tp::ContactInfoFieldList  listContactInfo = contact->infoFields().allFields();

    if (listContactInfo.count() == 0) {
        qWarning() << "No contact info present";
        return;
    }

    RDFStatementList inserts;

    Q_FOREACH (const Tp::ContactInfoField &field, listContactInfo) {
        if (field.fieldValue.count() == 0) {
            continue;
        }
        if (!field.fieldName.compare("tel")) {
            addContactVoicePhoneNumberToQuery(inserts,
                    createAffiliation(inserts, imContact, field),
                    field.fieldValue.at(0));
        } else if (!field.fieldName.compare("adr")) {
            addContactAddressToQuery(inserts,
                    createAffiliation(inserts, imContact, field),
                    field.fieldValue.at(0),
                    field.fieldValue.at(1),
                    field.fieldValue.at(2),
                    field.fieldValue.at(3),
                    field.fieldValue.at(4),
                    field.fieldValue.at(5),
                    field.fieldValue.at(6));
        }
    }

    query.addInsertion(inserts, graph);
}

RDFVariable CDTpStorage::createAffiliation(RDFStatementList &inserts,
        const RDFVariable &imContact,
        const Tp::ContactInfoField &field)
{
    static uint counter = 0;
    RDFVariable imAffiliation = RDFVariable(QString("affiliation%1").arg(++counter));
    inserts << RDFStatement(imAffiliation, rdf::type::iri(), nco::Affiliation::iri())
            << RDFStatement(imContact, nco::hasAffiliation::iri(), imAffiliation);

    Q_FOREACH (QString parameter, field.parameters) {
        if (parameter.startsWith("type=")) {
            inserts << RDFStatement(imAffiliation, rdfs::label::iri(), LiteralValue(parameter.mid(5)));
            break;
        }
    }

    return imAffiliation;
}

void CDTpStorage::addContactVoicePhoneNumberToQuery(RDFStatementList &inserts,
        const RDFVariable &imAffiliation,
        const QString &phoneNumber)
{
    RDFVariable voicePhoneNumber = QUrl(QString("tel:%1").arg(phoneNumber));
    inserts << RDFStatement(voicePhoneNumber, rdf::type::iri(), nco::VoicePhoneNumber::iri())
            << RDFStatement(voicePhoneNumber, maemo::localPhoneNumber::iri(), LiteralValue(phoneNumber))
            << RDFStatement(voicePhoneNumber, nco::phoneNumber::iri(), LiteralValue(phoneNumber));

    inserts << RDFStatement(imAffiliation, nco::hasPhoneNumber::iri(), voicePhoneNumber);
}

void CDTpStorage::addContactAddressToQuery(RDFStatementList &inserts,
        const RDFVariable &imAffiliation,
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
    inserts << RDFStatement(imPostalAddress, rdf::type::iri(), nco::PostalAddress::iri())
            << RDFStatement(imPostalAddress, nco::pobox::iri(), LiteralValue(pobox))
            << RDFStatement(imPostalAddress, nco::extendedAddress::iri(), LiteralValue(extendedAddress))
            << RDFStatement(imPostalAddress, nco::streetAddress::iri(), LiteralValue(streetAddress))
            << RDFStatement(imPostalAddress, nco::locality::iri(), LiteralValue(locality))
            << RDFStatement(imPostalAddress, nco::region::iri(), LiteralValue(region))
            << RDFStatement(imPostalAddress, nco::postalcode::iri(), LiteralValue(postalcode))
            << RDFStatement(imPostalAddress, nco::country::iri(), LiteralValue(country));

    inserts << RDFStatement(imAffiliation, nco::hasPostalAddress::iri(), imPostalAddress);
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
    CDTpAccount *accountWrapper = contactWrapper->accountWrapper();
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
    CDTpAccount *accountWrapper = contactWrapper->accountWrapper();
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();
    return contactImAddress(account->objectPath(), contact->id());
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

CDTpStorageSelectQuery::CDTpStorageSelectQuery(const RDFSelect &select,
        QObject *parent)
    : QObject(parent)
{
    mReply = ::tracker()->modelQuery(select);
    connect(mReply.model(),
            SIGNAL(modelUpdated()),
            SLOT(onModelUpdated()));
}

CDTpStorageSelectQuery::~CDTpStorageSelectQuery()
{
}

void CDTpStorageSelectQuery::onModelUpdated()
{
    Q_EMIT finished(this);
}

CDTpStorageContactResolver::CDTpStorageContactResolver(CDTpAccount *accountWrapper,
        const QList<CDTpContactPtr> &contactsToResolve,
        QObject *parent)
    : QObject(parent)
{
    mContactsNotResolved = contactsToResolve;
    requestContactResolve(accountWrapper, contactsToResolve);
}

CDTpStorageContactResolver::~CDTpStorageContactResolver()
{
    mContactsNotResolved.clear();
    mResolvedContacts.clear();
}

QList<CDTpContactPtr> CDTpStorageContactResolver::resolvedRemoteContacts() const
{
    if (mResolvedContacts.keys().empty()) {
        return mContactsNotResolved;
    }

    return mResolvedContacts.keys();
}

QList<CDTpContactPtr> CDTpStorageContactResolver::remoteContacts() const
{
    return mContactsNotResolved;
}

QString CDTpStorageContactResolver::storageIdForContact(CDTpContactPtr contactWrapper) const
{
    return mResolvedContacts[contactWrapper];
}

void CDTpStorageContactResolver::setContactChanges(CDTpContact::Changes changes)
{
    mContactChanges = changes;
}

CDTpContact::Changes CDTpStorageContactResolver::contactChanges() const
{
    return mContactChanges;
}

void CDTpStorageContactResolver::onStorageResolveSelectQueryFinished(
        CDTpStorageSelectQuery *queryWrapper)
{
    LiveNodes result = queryWrapper->reply();

    for (int i = 0; i < result->rowCount(); i++) {
        const QString storageContact = result->index(i, 2).data().toString();
        const QString imAddress = result->index(i, 1).data().toString();
        Q_FOREACH (CDTpContactPtr contactWrapper, mContactsNotResolved) {
            if (contactWrapper->contact()->id() == imAddress) {
                mResolvedContacts[contactWrapper] = storageContact;
            }
        }
    }

    Q_EMIT finished(this);
}

void CDTpStorageContactResolver::requestContactResolve(CDTpAccount *accountWrapper,
        const QList<CDTpContactPtr> &contactList)
{
    Q_UNUSED(accountWrapper);

    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAddress = imContact.property<nco::hasIMAddress>();

    RDFVariableList members;
    Q_FOREACH (CDTpContactPtr contactWrapper, contactList) {
        members << RDFVariable(CDTpStorage::contactImAddress(contactWrapper));
    }
    imAddress.isMemberOf(members);

    RDFSelect select;
    select.addColumn("contact", imContact);
    select.addColumn("distinct", imAddress.property<nco::imID>());
    select.addColumn("contactId", imContact.property<nco::contactLocalUID>());

    CDTpStorageSelectQuery *query = new CDTpStorageSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpStorageSelectQuery *)),
            SLOT(onStorageResolveSelectQueryFinished(CDTpStorageSelectQuery *)));
}

