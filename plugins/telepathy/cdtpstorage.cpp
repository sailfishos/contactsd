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

#include <TelepathyQt4/ContactCapabilities>

const QUrl CDTpStorage::defaultGraph = QUrl(
        QLatin1String("urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9"));

CDTpStorage::CDTpStorage(QObject *parent)
    : QObject(parent)
{
}

CDTpStorage::~CDTpStorage()
{
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
    const QUrl accountUrl(QString("telepathy:%1").arg(accountObjectPath));
    const QUrl imAddressUrl(QString("telepathy:%1!%2")
            .arg(accountObjectPath).arg(accountId));

    qDebug() << "Syncing account" << accountObjectPath << "to storage" << accountId;

    RDFUpdate up;

    RDFVariable imAccount(accountUrl);
    RDFVariable imAddress(imAddressUrl);
    RDFStatementList inserts;
    RDFStatementList deletions;

    deletions << RDFStatement(imAccount, nco::imAccountType::iri())
            << RDFStatement(imAddress, nco::imID::iri());
    inserts << RDFStatement(imAccount, rdf::type::iri(), nco::IMAccount::iri()) <<
        RDFStatement(imAccount, nco::imAccountType::iri(), LiteralValue(account->protocol())) <<
        RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri()) <<
        RDFStatement(imAddress, nco::imID::iri(), LiteralValue(account->normalizedName()));

    if (changes & CDTpAccount::DisplayName) {
        deletions << RDFStatement(imAccount, nco::imDisplayName::iri());
        inserts << RDFStatement(imAccount, nco::imDisplayName::iri(),
                LiteralValue(account->displayName()));
    }

    if (changes & CDTpAccount::Nickname) {
       deletions << RDFStatement(imAddress, nco::imNickname::iri());
       inserts << RDFStatement(imAddress, nco::imNickname::iri(),
               LiteralValue(account->nickname()));
    }

    if (changes & CDTpAccount::Presence) {
        Tp::SimplePresence presence = account->currentPresence();

        deletions << RDFStatement(imAddress, nco::imStatusMessage::iri()) <<
            RDFStatement(imAddress, nco::imPresence::iri()) <<
            RDFStatement(imAddress, nco::presenceLastModified::iri());
        inserts << RDFStatement(imAddress, nco::imStatusMessage::iri(),
                LiteralValue(presence.statusMessage)) <<
            RDFStatement(imAddress, nco::imPresence::iri(),
                    RDFVariable(trackerStatusFromTpPresenceStatus(presence.status))) <<
            RDFStatement(imAddress, nco::presenceLastModified::iri(),
                    LiteralValue(QDateTime::currentDateTime()));
    }

    // link the IMAddress to me-contact
    deletions << RDFStatement(nco::default_contact_me::iri(), nco::imAccountAddress::iri());

    inserts << RDFStatement(nco::default_contact_me::iri(),
            nco::hasIMAddress::iri(), imAddress) <<
        RDFStatement(imAccount, nco::imAccountAddress::iri(), imAddress) <<
        RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress);

    if (changes & CDTpAccount::Avatar) {
        const Tp::Avatar &avatar = account->avatar();
        // TODO: saving to disk needs to be removed here
        saveAccountAvatar(avatar.avatarData, avatar.MIMEType, imAddress,
            inserts, deletions);
    }

    up.addDeletion(deletions, defaultGraph);
    up.addInsertion(inserts, defaultGraph);

    ::tracker()->executeQuery(up);
}

void CDTpStorage::syncAccountContacts(CDTpAccount *accountWrapper)
{
    // TODO: return the number of contacts that were actually added
    syncAccountContacts(accountWrapper, accountWrapper->contacts(),
            QList<CDTpContact *>());
}

void CDTpStorage::syncAccountContacts(CDTpAccount *accountWrapper,
        const QList<CDTpContact *> &contactsAdded,
        const QList<CDTpContact *> &contactsRemoved)
{
    Tp::AccountPtr account = accountWrapper->account();
    QString accountObjectPath = account->objectPath();

    qDebug() << "Syncing account" << accountObjectPath <<
        "roster contacts to storage";
    qDebug() << " " << contactsAdded.size() << "contacts added";
    qDebug() << " " << contactsRemoved.size() << "contacts removed";

    CDTpStorageContactResolver *addResolver =
        new CDTpStorageContactResolver(accountWrapper, contactsAdded, this);
    CDTpStorageContactResolver *deleteResolver =
        new CDTpStorageContactResolver(accountWrapper, contactsRemoved, this);
    connect(addResolver,
            SIGNAL(finished(CDTpStorageContactResolver *)),
            SLOT(onContactAddResolverFinished(CDTpStorageContactResolver *)));
    connect(deleteResolver,
            SIGNAL(finished(CDTpStorageContactResolver *)),
            SLOT(onContactDeleteResolverFinished(CDTpStorageContactResolver *)));
}

void CDTpStorage::syncAccountContact(CDTpAccount *accountWrapper,
        CDTpContact *contactWrapper, CDTpContact::Changes changes)
{
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();

    CDTpStorageContactResolver *updateResolver =
        new CDTpStorageContactResolver(accountWrapper,
                QList<CDTpContact *>() << contactWrapper, this);
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
    RDFVariable imAccount = RDFVariable::fromType<nco::IMAccount>();

    imAccount.property<nco::hasIMContact>() == imAddress;
    imAccount == QUrl(QString("telepathy:%1").arg(account->objectPath()));

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

    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAddress = imContact.optional().property<nco::hasIMAddress>();
    RDFVariable imAccount = RDFVariable::fromType<nco::IMAccount>();
    imAccount.property<nco::hasIMContact>() = imAddress;
    imAccount = QUrl("telepathy:" + accountObjectPath);

    RDFSelect select;
    select.addColumn("contact", imContact);
    select.addColumn("distinct", imAddress.property<nco::imID>());
    select.addColumn("contactId", imContact.property<nco::contactLocalUID>());
    select.addColumn("accountPath", imAccount);
    select.addColumn("address", imAddress);

    CDTpStorageSelectQuery *query = new CDTpStorageSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpStorageSelectQuery *)),
            SLOT(onAccountRemovalSelectQueryFinished(CDTpStorageSelectQuery *)));
}

void CDTpStorage::onAccountRemovalSelectQueryFinished(CDTpStorageSelectQuery *query)
{
    RDFUpdate update;

    LiveNodes removalNodes = query->reply();
    for (int i = 0; i < removalNodes->rowCount(); ++i) {
        QUrl personContactIri = QUrl(removalNodes->index(i, 0).data().toString());
        const QString imStorageAddress = removalNodes->index(i, 1).data().toString();
        const QString contactLocalUID = removalNodes->index(i, 2).data().toString();
        const QString accountUri = removalNodes->index(i, 3).data().toString();
        const QString accountObjectPath(accountUri.split(":").value(1));
        QUrl imContactGeneratedIri = QUrl(contactImAddress(accountObjectPath,
                    imStorageAddress));
        const RDFVariable imContact(contactIri(contactLocalUID));
        const RDFVariable imAddress(contactImAddress(accountObjectPath, imStorageAddress));
        const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));

        if (personContactIri == imContactGeneratedIri) {
            update.addDeletion(imContact, rdf::type::iri(), nco::PersonContact::iri());
        }
        update.addDeletion(imAddress, rdf::type::iri(), nco::IMAddress::iri(),
                defaultGraph);
        update.addDeletion(imAccount, rdf::type::iri(), nco::IMAccount::iri(),
                defaultGraph);
    }

    ::tracker()->executeQuery(update);
    query->deleteLater();
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

        update.addDeletion(imAddress, nco::imPresence::iri());
        update.addDeletion(imAddress, nco::presenceLastModified::iri());

        update.addInsertion(imAddress, nco::imPresence::iri(),
                unknownState);
        update.addInsertion(imAddress, nco::presenceLastModified::iri(),
                LiteralValue(QDateTime::currentDateTime()));
        update.addDeletion(imContact, nie::contentLastModified::iri());
        update.addInsertion(imContact, nie::contentLastModified::iri(),
            LiteralValue(QDateTime::currentDateTime()));
    }

    ::tracker()->executeQuery(update);
}

void CDTpStorage::onContactAddResolverFinished(CDTpStorageContactResolver *resolver)
{
    RDFUpdate updateQuery;
    RDFStatementList deletions;
    RDFStatementList inserts;

    foreach (CDTpContact *contactWrapper, resolver->remoteContacts()) {
        Tp::ContactPtr contact = contactWrapper->contact();
        QString accountObjectPath =
            contactWrapper->accountWrapper()->account()->objectPath();

        const QString id = contact->id();
        QString localId = resolver->storageIdForContact(contactWrapper);
        bool alreadyExists = !localId.isEmpty();

        if (!alreadyExists) {
            localId = contactLocalId(accountObjectPath, id);
        }

        const RDFVariable imContact(contactIri(localId));
        const RDFVariable imAddress(contactImAddress(accountObjectPath, id));
        const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
        const QDateTime datetime = QDateTime::currentDateTime();

        /* Insert the imContact only if we didn't found one. UI could already
         * have created the imContact before adding the im contact in telepathy
         */
        if (!alreadyExists) {
            deletions << RDFStatement(imContact, nie::contentCreated::iri());
            inserts << RDFStatement(imContact, rdf::type::iri(), nco::PersonContact::iri()) <<
                RDFStatement(imContact, nco::hasIMAddress::iri(), imAddress) <<
                RDFStatement(imContact, nco::contactLocalUID::iri(), LiteralValue(localId)) <<
                RDFStatement(imContact, nco::contactUID::iri(), LiteralValue(localId)) <<
                RDFStatement(imContact, nie::contentCreated::iri(), LiteralValue(datetime)) <<
                RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime)) <<
                RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri()) <<
                RDFStatement(imAddress, nco::imID::iri(), LiteralValue(id));
        } else {
            deletions << RDFStatement(imContact, nie::contentLastModified::iri());
            inserts << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime));
        }

        inserts << RDFStatement(imContact, rdf::type::iri(), nco::PersonContact::iri()) <<
            RDFStatement(imContact, nco::hasIMAddress::iri(), imAddress) <<
            RDFStatement(imAccount, rdf::type::iri(), nco::IMAccount::iri()) <<
            RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress);
        addContactAliasInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        addContactPresenceInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        addContactCapabilitiesInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        addContactAvatarInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        addContactAuthorizationInfoToQuery(deletions, inserts, imAddress, contactWrapper);
    }

    updateQuery.addDeletion(deletions, defaultGraph);
    updateQuery.addInsertion(inserts, defaultGraph);
    ::tracker()->executeQuery(updateQuery);
    resolver->deleteLater();
}

void CDTpStorage::onContactDeleteResolverFinished(CDTpStorageContactResolver *resolver)
{
    RDFUpdate updateQuery;
    RDFStatementList deletions;
    RDFStatementList inserts;

    foreach (CDTpContact *contactWrapper, resolver->remoteContacts()) {
        Tp::ContactPtr contact = contactWrapper->contact();
        QString accountObjectPath =
            contactWrapper->accountWrapper()->account()->objectPath();

        const QString id = contact->id();
        QString localId = resolver->storageIdForContact(contactWrapper);

        if (localId.isEmpty() || localId.isNull()) {
            localId = contactLocalId(accountObjectPath, id);
        }

        addContactRemoveInfoToQuery(deletions,
                inserts,
                localId,
                contactWrapper->accountWrapper(),
                contactWrapper);
    }

    updateQuery.addDeletion(deletions, defaultGraph);
    updateQuery.addInsertion(inserts, defaultGraph);
    ::tracker()->executeQuery(updateQuery);
    resolver->deleteLater();
}

void CDTpStorage::onContactUpdateResolverFinished(CDTpStorageContactResolver *resolver)
{
    RDFUpdate updateQuery;
    RDFStatementList deletions;
    RDFStatementList inserts;

    foreach (CDTpContact *contactWrapper, resolver->remoteContacts()) {
        Tp::ContactPtr contact = contactWrapper->contact();
        QString accountObjectPath =
            contactWrapper->accountWrapper()->account()->objectPath();

        const QString id = contact->id();
        QString localId = resolver->storageIdForContact(contactWrapper);

        if (localId.isEmpty() || localId.isNull()) {
            localId = contactLocalId(accountObjectPath, id);
        }
        qDebug() << Q_FUNC_INFO << "Updating " << localId;

        const RDFVariable imContact(contactIri(localId));
        const RDFVariable imAddress(contactImAddress(accountObjectPath, id));
        const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));
        const QDateTime datetime = QDateTime::currentDateTime();

        deletions << RDFStatement(imContact, nie::contentLastModified::iri());
        inserts << RDFStatement(imContact, nie::contentLastModified::iri(), LiteralValue(datetime));
        const CDTpContact::Changes changes = resolver->contactChanges();

        if (changes & CDTpContact::Alias) {
            qDebug() << "  alias changed";
            addContactAliasInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Presence) {
            qDebug() << "  presence changed";
            addContactPresenceInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Capabilities) {
            qDebug() << "  capabilities changed";
            addContactCapabilitiesInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Avatar) {
            qDebug() << "  avatar changed";
           addContactAvatarInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        }
        if (changes & CDTpContact::Authorization) {
            qDebug() << "  authorization changed";
            addContactAuthorizationInfoToQuery(deletions, inserts, imAddress, contactWrapper);
        }
    }

    updateQuery.addDeletion(deletions, defaultGraph);
    updateQuery.addInsertion(inserts, defaultGraph);
    ::tracker()->executeQuery(updateQuery);
    resolver->deleteLater();
}

void CDTpStorage::saveAccountAvatar(const QByteArray &data, const QString &mimeType,
        const RDFVariable &imAddress, RDFStatementList &deletions,
        RDFStatementList &inserts)
{
    Q_UNUSED(mimeType);

    deletions << RDFStatement(imAddress, nco::imAvatar::iri());

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
    deletions << RDFStatement(dataObject, nie::url::iri());
    inserts << RDFStatement(dataObject, rdf::type::iri(), nie::DataObject::iri())
            << RDFStatement(dataObject, nie::url::iri(), dataObject)
            << RDFStatement(imAddress, nco::imAvatar::iri(), dataObject);
}

void CDTpStorage::addContactAliasInfoToQuery(RDFStatementList &deletions,
        RDFStatementList &inserts,
        const RDFVariable &imAddress,
        CDTpContact *contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    deletions << RDFStatement(imAddress, nco::imNickname::iri());
    inserts << RDFStatement(imAddress, nco::imNickname::iri(),
                LiteralValue(contact->alias()));
}

void CDTpStorage::addContactPresenceInfoToQuery(RDFStatementList &deletions,
        RDFStatementList &inserts,
        const RDFVariable &imAddress,
        CDTpContact *contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    deletions << RDFStatement(imAddress, nco::imPresence::iri()) <<
            RDFStatement(imAddress, nco::imStatusMessage::iri()) <<
            RDFStatement(imAddress, nco::presenceLastModified::iri());

    inserts << RDFStatement(imAddress, nco::imStatusMessage::iri(),
                LiteralValue(contact->presenceMessage())) <<
            RDFStatement(imAddress, nco::imPresence::iri(),
                trackerStatusFromTpPresenceType(contact->presenceType())) <<
            RDFStatement(imAddress, nco::presenceLastModified::iri(),
                RDFVariable(QDateTime::currentDateTime()));
}

void CDTpStorage::addContactCapabilitiesInfoToQuery(RDFStatementList &deletions,
        RDFStatementList &inserts,
        const RDFVariable &imAddress,
        CDTpContact *contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    deletions << RDFStatement(imAddress, nco::imCapability::iri());

    if (contact->capabilities()->supportsTextChats()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_text_chat::iri());
    }

    if (contact->capabilities()->supportsAudioCalls()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_audio_calls::iri());
    }

    if (contact->capabilities()->supportsVideoCalls()) {
        inserts << RDFStatement(imAddress, nco::imCapability::iri(),
                    nco::im_capability_video_calls::iri());
    }
}

void CDTpStorage::addContactAvatarInfoToQuery(RDFStatementList &deletions,
        RDFStatementList &inserts,
        const RDFVariable &imAddress,
        CDTpContact *contactWrapper)
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
    deletions << RDFStatement(imAddress, nco::imAvatar::iri()) <<
        RDFStatement(dataObject, nie::url::iri());

    if (!contact->avatarToken().isEmpty()) {
        inserts << RDFStatement(dataObject, rdf::type::iri(), nie::DataObject::iri()) <<
            RDFStatement(dataObject, nie::url::iri(), dataObject) <<
            RDFStatement(imAddress, nco::imAvatar::iri(), dataObject);
    }
}

QUrl CDTpStorage::authStatus(Tp::Contact::PresenceState state) const
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

void CDTpStorage::addContactAuthorizationInfoToQuery(RDFStatementList &deletions,
        RDFStatementList &inserts,
        const RDFVariable &imAddress,
        CDTpContact *contactWrapper)
{
    Tp::ContactPtr contact = contactWrapper->contact();

    deletions << RDFStatement(imAddress, nco::imAddressAuthStatusFrom::iri());
    inserts << RDFStatement(imAddress, nco::imAddressAuthStatusFrom::iri(),
        RDFVariable(authStatus(contact->subscriptionState())));

    deletions << RDFStatement(imAddress, nco::imAddressAuthStatusTo::iri());
    inserts << RDFStatement(imAddress, nco::imAddressAuthStatusTo::iri(),
        RDFVariable(authStatus(contact->publishState())));
}

void CDTpStorage::addContactRemoveInfoToQuery(RDFStatementList &deletions,
        RDFStatementList &inserts,
        const QString &contactId,
        CDTpAccount *accountWrapper,
        CDTpContact *contactWrapper)
{
    Q_UNUSED(inserts);

    Tp::ContactPtr contact = contactWrapper->contact();
    QString accountObjectPath = accountWrapper->account()->objectPath();
    const QString id = contact->id();
    QUrl imContactIri = QUrl(contactId);
    QUrl hashId = QUrl(QString("contact:%1")
            .arg((contactLocalId(accountObjectPath, id))));
    const RDFVariable imContact(imContactIri);
    const RDFVariable imAddress(contactImAddress(accountObjectPath, id));
    const RDFVariable imAccount(QUrl(QString("telepathy:%1").arg(accountObjectPath)));

    // avoid deleting when contacts are merged
    if (hashId == imContactIri) {
        deletions << RDFStatement(imContact, rdf::type::iri(), nco::PersonContact::iri());
    }

    deletions << RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress) <<
        RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri());
}

QString CDTpStorage::contactLocalId(const QString &contactAccountObjectPath,
        const QString &contactId) const
{
    return QString::number(qHash(QString("%1!%2")
                .arg(contactAccountObjectPath)
                .arg(contactId)));
}

QString CDTpStorage::contactLocalId(CDTpContact *contactWrapper) const
{
    CDTpAccount *accountWrapper = contactWrapper->accountWrapper();
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();
    return contactLocalId(account->objectPath(), contact->id());
}

QUrl CDTpStorage::contactIri(const QString &contactLocalId) const
{
    return QUrl(QString("contact:%1").arg(contactLocalId));
}

QUrl CDTpStorage::contactIri(CDTpContact *contactWrapper) const
{
    return contactIri(contactLocalId(contactWrapper));
}

QUrl CDTpStorage::contactImAddress(const QString &contactAccountObjectPath,
        const QString &contactId) const
{
    return QUrl(QString("telepathy:%1!%2")
            .arg(contactAccountObjectPath)
            .arg(contactId));
}

QUrl CDTpStorage::contactImAddress(CDTpContact *contactWrapper) const
{
    CDTpAccount *accountWrapper = contactWrapper->accountWrapper();
    Tp::AccountPtr account = accountWrapper->account();
    Tp::ContactPtr contact = contactWrapper->contact();
    return contactImAddress(account->objectPath(), contact->id());
}

QUrl CDTpStorage::trackerStatusFromTpPresenceType(uint tpPresenceType) const
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
        const QString &tpPresenceStatus) const
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
    emit finished(this);
}

CDTpStorageContactResolver::CDTpStorageContactResolver(CDTpAccount *accountWrapper,
        const QList<CDTpContact *> &contactsToResolve,
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

QList<CDTpContact *> CDTpStorageContactResolver::resolvedRemoteContacts() const
{
    if (mResolvedContacts.keys().empty()) {
        return mContactsNotResolved;
    }

    return mResolvedContacts.keys();
}

QList<CDTpContact *> CDTpStorageContactResolver::remoteContacts() const
{
    return mContactsNotResolved;
}

QString CDTpStorageContactResolver::storageIdForContact(CDTpContact *contactWrapper) const
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
        foreach (CDTpContact *contactWrapper, mContactsNotResolved) {
            if (contactWrapper->contact()->id() == imAddress) {
                mResolvedContacts[contactWrapper] = storageContact;
            }
        }
    }

    emit finished(this);
}

void CDTpStorageContactResolver::requestContactResolve(CDTpAccount *accountWrapper,
        const QList<CDTpContact *> &contactList)
{
    RDFVariable imContact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imAddress = imContact.property<nco::hasIMAddress>();
    RDFVariable imAccount = RDFVariable::fromType<nco::IMAccount>();
    QString accountObjectPath = accountWrapper->account()->objectPath();
    RDFVariableList members;

    RDFSelect select;
    imAccount = QUrl("telepathy:" + accountObjectPath);
    imAccount.property<nco::hasIMContact>() = imAddress;

    foreach (CDTpContact *contactWrapper, contactList) {
        QString storageUri = QString("telepathy:%1!%2")
            .arg(accountObjectPath)
            .arg(contactWrapper->contact()->id());
        members << RDFVariable(QUrl(storageUri));
    }

    imAddress.isMemberOf(members);

    select.addColumn("contact", imContact);
    select.addColumn("distinct", imAddress.property<nco::imID>());
    select.addColumn("contactId", imContact.property<nco::contactLocalUID>());
    select.addColumn("accountPath", imAccount);
    select.addColumn("address", imAddress);

    CDTpStorageSelectQuery *query = new CDTpStorageSelectQuery(select, this);
    connect(query,
            SIGNAL(finished(CDTpStorageSelectQuery *)),
            SLOT(onStorageResolveSelectQueryFinished(CDTpStorageSelectQuery *)));
}
