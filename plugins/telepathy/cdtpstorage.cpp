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

#include <TelepathyQt4/AvatarData>
#include <TelepathyQt4/ContactCapabilities>
#include <TelepathyQt4/ConnectionCapabilities>

#include "cdtpstorage.h"

#define MAX_UPDATE_SIZE 999
#define MAX_REMOVE_SIZE 999

const QString CDTpStorage::defaultGenerator = "\"telepathy\"";

CDTpStorage::CDTpStorage(QObject *parent)
    : QObject(parent)
{
    mQueueTimer.setSingleShot(true);
    mQueueTimer.setInterval(100);
    connect(&mQueueTimer, SIGNAL(timeout()), SLOT(onQueueTimerTimeout()));
}

CDTpStorage::~CDTpStorage()
{
}

void CDTpStorage::syncAccounts(const QList<CDTpAccountPtr> &accounts)
{
    CDTpQueryBuilder builder;

    /* Purge accounts and their contacts that does not exist anymore */
    QStringList imAccounts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        imAccounts << literalIMAccount(accountWrapper);
    }

    /* Bind imAccount to all accounts that does not exist anymore,
     * and imAddress to all contacts of this imAccount */
    const QString imAccount = builder.uniquify("?imAccount");
    const QString imAddress = builder.uniquify("?imAddress");
    const QString selection = QString(QLatin1String(
            "%1 a nco:IMAccount.\n"
            "FILTER (%1 NOT IN (%2)).\n"
            "%3 a nco:IMAddress.\n"
            "%1 nco:hasIMContact %3."))
            .arg(imAccount).arg(imAccounts.join(",")).arg(imAddress);

    /* Delete/update local contact */
    builder.appendRawSelection(selection);
    addRemoveContactToBuilder(builder, imAddress);

    /* Delete imAddress and imAccount in a sub builder because they are needed
     * for the selection of builder. */
    CDTpQueryBuilder subBuilder;
    subBuilder.appendRawSelection(selection);
    subBuilder.deleteResource(imAddress);
    subBuilder.deleteResource(imAccount);
    builder.appendRawQuery(subBuilder);

    /* Sync accounts and their contacts */
    subBuilder.clear();
    addCreateAccountsToBuilder(subBuilder, accounts);
    builder.appendRawQuery(subBuilder);

    new CDTpSparqlQuery(builder.getSparqlQuery(), this);
}

void CDTpStorage::syncAccount(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder;
    addCreateAccountsToBuilder(builder, QList<CDTpAccountPtr>() << accountWrapper);
    new CDTpSparqlQuery(builder.getSparqlQuery(), this);
}

void CDTpStorage::updateAccount(CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes)
{
    CDTpQueryBuilder builder;
    addUpdateAccountToBuilder(builder, accountWrapper, changes);
    new CDTpSparqlQuery(builder.getSparqlQuery(), this);
}

void CDTpStorage::removeAccount(CDTpAccountPtr accountWrapper)
{
    const QString imAccount = literalIMAccount(accountWrapper);
    qDebug() << "Removing account" << imAccount;

    CDTpQueryBuilder builder;

    /* Bind imAddress to all contacts of this imAccount */
    const QString imAddress = builder.uniquify("?imAddress");
    const QString selection = (QString(QLatin1String(
            "%1 a nco:IMAddress.\n"
            "%2 nco:hasIMContact %1.")).arg(imAddress).arg(imAccount));

    /* Delete/update local contact */
    builder.appendRawSelection(selection);
    addRemoveContactToBuilder(builder, imAddress);

    /* Delete imAddress and imAccount in a sub builder because they are needed
     * for the selection of builder. */
    CDTpQueryBuilder subBuilder;
    subBuilder.appendRawSelection(selection);
    subBuilder.deleteResource(imAddress);
    subBuilder.deleteResource(imAccount);
    builder.appendRawQuery(subBuilder);

    new CDTpSparqlQuery(builder.getSparqlQuery(), this);
}

void CDTpStorage::addCreateAccountsToBuilder(CDTpQueryBuilder &builder,
        const QList<CDTpAccountPtr> &accounts) const
{
    QStringList imAddresses;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        Tp::AccountPtr account = accountWrapper->account();
        const QString imAccount = literalIMAccount(accountWrapper);
        const QString imAddress = literalIMAddress(accountWrapper);

        qDebug() << "Syncing account" << imAddress;

        // Ensure the IMAccount exists
        builder.createResource(imAccount, "nco:IMAccount", CDTpQueryBuilder::privateGraph);
        builder.insertProperty(imAccount, "nco:imAccountType", literal(account->protocolName()), CDTpQueryBuilder::privateGraph);
        builder.insertProperty(imAccount, "nco:imAccountAddress", imAddress, CDTpQueryBuilder::privateGraph);
        builder.insertProperty(imAccount, "nco:hasIMContact", imAddress, CDTpQueryBuilder::privateGraph);

        // Ensure the self contact has an IMAddress
        builder.createResource(imAddress, "nco:IMAddress");
        builder.insertProperty(imAddress, "nco:imID", literal(account->normalizedName()));
        builder.insertProperty(imAddress, "nco:imProtocol", literal(account->protocolName()));

        // Assume everything changed
        addUpdateAccountToBuilder(builder, accountWrapper, CDTpAccount::All);

        // Create account's contacts
        CDTpQueryBuilder subBuilder;
        addSyncAccountContactsToBuilder(subBuilder, accountWrapper);
        builder.appendRawQuery(subBuilder);

        imAddresses << imAddress;
    }

    // Ensure the IMAddresses are bound to the default-contact-me via an affiliation
    builder.appendRawQuery(QString(QLatin1String(
        "INSERT {\n"
        "    GRAPH %1 {\n"
        "        nco:default-contact-me nco:hasAffiliation _:affiliation.\n"
        "        _:affiliation a nco:Affiliation;\n"
        "                      nco:hasIMAddress ?imAddress;\n"
        "                      rdfs:label \"Others\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    ?imAddress a nco:IMAddress.\n"
        "    FILTER (?imAddress IN (%2) &&\n"
        "            NOT EXISTS { nco:default-contact-me nco:hasAffiliation [ nco:hasIMAddress ?imAddress ] })\n"
        "}\n"))
        .arg(CDTpQueryBuilder::defaultGraph).arg(imAddresses.join(",")));
}

void CDTpStorage::addUpdateAccountToBuilder(CDTpQueryBuilder &builder,
        CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes) const
{
    Tp::AccountPtr account = accountWrapper->account();
    const QString imAccount = literalIMAccount(accountWrapper);
    const QString imAddress = literalIMAddress(accountWrapper);

    qDebug() << "Update account" << imAddress;

    if (changes & CDTpAccount::Nickname) {
        qDebug() << "  nickname changed";
        builder.updateProperty(imAddress, "nco:imNickname", literal(account->nickname()));
    }
    if (changes & CDTpAccount::Presence) {
        qDebug() << "  presence changed";
        addPresenceToBuilder(builder, imAddress, account->currentPresence());
    }
    if (changes & CDTpAccount::Avatar) {
        qDebug() << "  avatar changed";
        addAccountAvatarToBuilder(builder, imAddress, account->avatar());
    }
    if (changes & CDTpAccount::DisplayName) {
        qDebug() << "  display name changed";
        builder.updateProperty(imAccount, "nco:imDisplayName", literal(account->displayName()),
                CDTpQueryBuilder::privateGraph);
    }

    builder.updateProperty("nco:default-contact-me", "nie:contentLastModified", literalTimeStamp());
}

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder;
    addSyncAccountContactsToBuilder(builder, accountWrapper);
    new CDTpSparqlQuery(builder.getSparqlQuery(), this);
}

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contactsAdded,
        const QList<CDTpContactPtr> &contactsRemoved)
{
    CDTpQueryBuilder builder;

    if (!contactsAdded.isEmpty()) {
        addCreateContactsToBuilder(builder, contactsAdded);
    }

    if (!contactsRemoved.isEmpty()) {
        Q_FOREACH (const CDTpContactPtr &contactWrapper, contactsRemoved) {
            qDebug() << "Removing contact, cancel update:" << literalIMAddress(contactWrapper);
            mUpdateQueue.remove(contactWrapper);
        }
        addRemoveContactsToBuilder(builder, accountWrapper, contactsRemoved);
    }

    new CDTpSparqlQuery(builder.getSparqlQuery(), this);
}

void CDTpStorage::updateContact(CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    queueContactUpdate(contactWrapper, changes);
}

void CDTpStorage::addSyncAccountContactsToBuilder(CDTpQueryBuilder &builder,
        CDTpAccountPtr accountWrapper) const
{
    /* If account does not have a roster, mark all its contacts with UNKNOWN
     * presence/caps. */
    if (!accountWrapper->hasRoster()) {
        addSetContactsUnknownToBuilder(builder, accountWrapper);
        return;
    }

    /* Purge contacts not in the account anymore */
    QStringList imAddresses;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, accountWrapper->contacts()) {
        imAddresses << literalIMAddress(contactWrapper);
    }
    imAddresses << literalIMAddress(accountWrapper);

    /* Bind imAddress to all contacts that does not exist anymore */
    const QString imAccount = literalIMAccount(accountWrapper);
    const QString imAddress = builder.uniquify("?imAddress");
    const QString selection = QString(QLatin1String(
            "%1 a nco:IMAddress.\n"
            "%2 nco:hasIMContact %1.\n"
            "FILTER (%1 NOT IN (%3))."))
            .arg(imAddress).arg(imAccount).arg(imAddresses.join(","));

    builder.appendRawSelection(selection);
    addRemoveContactToBuilder(builder, imAddress);

    /* Delete imAddress and imAccount in a sub builder because they are needed
     * for the selection of builder. */
    CDTpQueryBuilder subBuilder;
    subBuilder.appendRawSelection(selection);
    subBuilder.deleteResource(imAddress);
    subBuilder.deleteProperty(imAccount, "nco:hasIMContact", imAddress);
    builder.appendRawQuery(subBuilder);

    subBuilder.clear();
    addCreateContactsToBuilder(subBuilder, accountWrapper->contacts());
    builder.appendRawQuery(subBuilder);
}

void CDTpStorage::queueContactUpdate(CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    if (!mUpdateQueue.contains(contactWrapper)) {
        qDebug() << "queue update for" << contactWrapper->contact()->id();
        mUpdateQueue.insert(contactWrapper, changes);
    } else {
        mUpdateQueue[contactWrapper] |= changes;
    }

    if (!mQueueTimer.isActive()) {
        mQueueTimer.start();
    }
}

void CDTpStorage::onQueueTimerTimeout()
{
    if (mUpdateQueue.isEmpty()) {
        return;
    }

    CDTpQueryBuilder builder;

    QHash<CDTpContactPtr, CDTpContact::Changes>::const_iterator i;
    for (i = mUpdateQueue.constBegin(); i != mUpdateQueue.constEnd(); ++i) {
        addUpdateContactToBuilder(builder, i.key(), i.value());
    }
    mUpdateQueue.clear();

    new CDTpSparqlQuery(builder.getSparqlQuery(), this);
}

void CDTpStorage::addCreateContactsToBuilder(CDTpQueryBuilder &builder,
        const QList<CDTpContactPtr> &contacts) const
{
    // Ensure all imAddress exists and are linked from imAccount
    QStringList imAddresses;
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();

        const QString imAddress = literalIMAddress(contactWrapper);
        builder.createResource(imAddress, "nco:IMAddress");
        builder.insertProperty(imAddress, "nco:imID", literal(contactWrapper->contact()->id()));
        builder.insertProperty(imAddress, "nco:imProtocol", literal(accountWrapper->account()->protocolName()));

        const QString imAccount = literalIMAccount(accountWrapper);
        builder.insertProperty(imAccount, "nco:hasIMContact", imAddress, CDTpQueryBuilder::privateGraph);

        imAddresses << imAddress;
    }

    // Ensure all imAddresses are bound to a PersonContact
    builder.appendRawQuery(QString(QLatin1String(
        "INSERT {\n"
        "    GRAPH %1 {\n"
        "        _:contact a nco:PersonContact;\n"
        "                  nco:hasAffiliation _:affiliation;\n"
        "                  nie:contentCreated %2;\n"
        "                  nie:generator %3 .\n"
        "        _:affiliation a nco:Affiliation;\n"
        "                      nco:hasIMAddress ?imAddress;\n"
        "                      rdfs:label \"Others\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    ?imAddress a nco:IMAddress.\n"
        "    FILTER (?imAddress IN (%4) &&\n"
        "            NOT EXISTS { ?contact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ] })\n"
        "}\n\n"))
        .arg(CDTpQueryBuilder::defaultGraph).arg(literalTimeStamp())
        .arg(defaultGenerator).arg(imAddresses.join(QLatin1String(","))));

    // Assume everything changed
    CDTpQueryBuilder subBuilder;
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        addUpdateContactToBuilder(subBuilder, contactWrapper, CDTpContact::All);
    }
    builder.appendRawQuery(subBuilder);
}

void CDTpStorage::addUpdateContactToBuilder(CDTpQueryBuilder &builder,
        CDTpContactPtr contactWrapper,
        CDTpContact::Changes changes) const
{
    const Tp::ContactPtr contact = contactWrapper->contact();
    const QString imAddress = literalIMAddress(contactWrapper);
    const QString imContact = builder.uniquify("?imContact");

    qDebug() << "Update contact" << imAddress;

    // bind imContact to imAddress
    builder.appendRawSelection(QString(QLatin1String(
            "%1 a nco:PersonContact.\n"
            "%1 nco:hasAffiliation [ nco:hasIMAddress %2 ]."))
            .arg(imContact).arg(imAddress));

    // Apply changes
    if (changes & CDTpContact::Alias) {
        qDebug() << "  alias changed";
        addContactAliasToBuilder(builder, imAddress, contactWrapper);
    }
    if (changes & CDTpContact::Presence) {
        qDebug() << "  presence changed";
        addPresenceToBuilder(builder, imAddress, contact->presence());
    }
    if (changes & CDTpContact::Capabilities) {
        qDebug() << "  capabilities changed";
        addCapabilitiesToBuilder(builder, imAddress, contact->capabilities());
    }
    if (changes & CDTpContact::Avatar) {
        qDebug() << "  avatar changed";
        addContactAvatarToBuilder(builder, imAddress, contactWrapper);
    }
    if (changes & CDTpContact::Authorization) {
        qDebug() << "  authorization changed";
        addContactAuthorizationToBuilder(builder, imAddress, contactWrapper);
    }
    if (changes & CDTpContact::Infomation) {
        qDebug() << "  vcard information changed";
        addContactInfoToBuilder(builder, imAddress, imContact, contactWrapper);
    }

    builder.updateProperty(imContact, "nie:contentLastModified", literalTimeStamp());
}

void CDTpStorage::addContactAliasToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        CDTpContactPtr contactWrapper) const
{
    Tp::ContactPtr contact = contactWrapper->contact();
    builder.updateProperty(imAddress, "nco:imNickname", literal(contact->alias()));
}

void CDTpStorage::addPresenceToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const Tp::Presence &presence) const
{
    builder.updateProperty(imAddress, "nco:imPresence", presenceType(presence.type()));
    builder.updateProperty(imAddress, "nco:imStatusMessage", literal(presence.statusMessage()));
    builder.updateProperty(imAddress, "nco:presenceLastModified", literalTimeStamp());
}

void CDTpStorage::addCapabilitiesToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        Tp::CapabilitiesBase capabilities) const
{
    builder.deleteProperty(imAddress, "nco:imCapability");

    if (capabilities.textChats()) {
        builder.insertProperty(imAddress, "nco:imCapability", "nco:im-capability-text-chat");
    }
    if (capabilities.streamedMediaAudioCalls()) {
        builder.insertProperty(imAddress, "nco:imCapability", "nco:im-capability-audio-calls");
    }
    if (capabilities.streamedMediaVideoCalls()) {
        builder.insertProperty(imAddress, "nco:imCapability", "nco:im-capability-video-calls");
    }
}

void CDTpStorage::addContactAvatarToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        CDTpContactPtr contactWrapper) const
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

    /* Remove current avatar */
    builder.deletePropertyAndLinkedResource(imAddress, "nco:imAvatar");

    /* Insert new avatar */
    if (!contact->avatarToken().isEmpty()) {
        const QString dataObject = builder.uniquify("_:dataObject");
        builder.createResource(dataObject, "nie:DataObject");
        builder.insertProperty(dataObject, "nie:url", literal(contact->avatarData().fileName));
        builder.insertProperty(imAddress, "nco:imAvatar", dataObject);
    }
}

void CDTpStorage::addAccountAvatarToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress, const Tp::Avatar &avatar) const
{
    builder.deletePropertyAndLinkedResource(imAddress, "nco:imAvatar");

    if (avatar.avatarData.isEmpty()) {
        return;
    }

    /* FIXME: Save data on disk, but AM should give us a filename, really */
    QString fileName = QString("%1/.contacts/avatars/%2")
        .arg(QDir::homePath())
        .arg(QString(QCryptographicHash::hash(avatar.avatarData, QCryptographicHash::Sha1).toHex()));
    qDebug() << "Saving account avatar to" << fileName;

    QFile avatarFile(fileName);
    if (!avatarFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to save account avatar: error opening avatar "
            "file" << fileName << "for writing";
        return;
    }
    avatarFile.write(avatar.avatarData);
    avatarFile.close();

    const QString dataObject = builder.uniquify("_:dataObject");
    builder.createResource(dataObject, "nie:DataObject");
    builder.insertProperty(dataObject, "nie:url", literal(fileName));
    builder.insertProperty(imAddress, "nco:imAvatar", dataObject);
}

void CDTpStorage::addContactAuthorizationToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        CDTpContactPtr contactWrapper) const
{
    Tp::ContactPtr contact = contactWrapper->contact();
    builder.updateProperty(imAddress, "nco:imAddressAuthStatusFrom", presenceState(contact->subscriptionState()));
    builder.updateProperty(imAddress, "nco:imAddressAuthStatusTo", presenceState(contact->publishState()));
}

void CDTpStorage::addContactInfoToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &imContact,
        CDTpContactPtr contactWrapper) const
{
    /* Use the imAddress as graph for ContactInfo fields, so we can easilly
     * know from which contact it comes from */
    const QString graph = imAddress;

    /* Drop current info */
    addRemoveContactInfoToBuilder(builder, imContact, graph);

    Tp::ContactPtr contact = contactWrapper->contact();
    Tp::ContactInfoFieldList listContactInfo = contact->infoFields().allFields();
    if (listContactInfo.count() == 0) {
        qDebug() << "No contact info present";
        return;
    }

    QHash<QString, QString> affiliations;
    Q_FOREACH (const Tp::ContactInfoField &field, listContactInfo) {
        if (field.fieldValue.count() == 0) {
            continue;
        }

        /* FIXME:
         *  - Do we care about "fn" and "nickname" ?
         *  - How do we write affiliation for "org" ?
         */

        if (!field.fieldName.compare("tel")) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            const QString voicePhoneNumber = builder.uniquify("_:tel");
            builder.createResource(voicePhoneNumber, "nco:VoicePhoneNumber", graph);
            builder.insertProperty(voicePhoneNumber, "maemo:localPhoneNumber", literalContactInfo(field, 0), graph);
            builder.insertProperty(voicePhoneNumber, "nco:phoneNumber", literalContactInfo(field, 0), graph);
            builder.insertProperty(affiliation, "nco:hasPhoneNumber", voicePhoneNumber, graph);
        }

        else if (!field.fieldName.compare("adr")) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            const QString postalAddress = builder.uniquify("_:address");
            builder.createResource(postalAddress, "nco:PostalAddress", graph);
            builder.insertProperty(postalAddress, "nco:pobox",           literalContactInfo(field, 0), graph);
            builder.insertProperty(postalAddress, "nco:extendedAddress", literalContactInfo(field, 1), graph);
            builder.insertProperty(postalAddress, "nco:streetAddress",   literalContactInfo(field, 2), graph);
            builder.insertProperty(postalAddress, "nco:locality",        literalContactInfo(field, 3), graph);
            builder.insertProperty(postalAddress, "nco:region",          literalContactInfo(field, 4), graph);
            builder.insertProperty(postalAddress, "nco:postalcode",      literalContactInfo(field, 5), graph);
            builder.insertProperty(postalAddress, "nco:country",         literalContactInfo(field, 6), graph);
            builder.insertProperty(affiliation, "nco:hasPostalAddress", postalAddress, graph);
        }

        else if (!field.fieldName.compare("email")) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            const QString emailAddress = builder.uniquify("_:email");
            builder.createResource(emailAddress, "nco:EmailAddress", graph);
            builder.insertProperty(emailAddress, "nco:emailAddress", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare("url")) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:url", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare("title")) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:title", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare("role")) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:role", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare("note") || !field.fieldName.compare("desc")) {
            builder.insertProperty(imContact, "nco:note", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare("bday")) {
            /* Tracker will reject anything not [-]CCYY-MM-DDThh:mm:ss[Z|(+|-)hh:mm]
             * VCard spec allows only ISO 8601, but most IM clients allows
             * any string. */
            /* FIXME: support more date format for compatibility */
            QDate date = QDate::fromString(field.fieldValue[0], "yyyy-MM-dd");
            if (!date.isValid()) {
                date = QDate::fromString(field.fieldValue[0], "yyyyMMdd");
            }

            if (date.isValid()) {
                builder.insertProperty(imContact, "nco:birthDate", literal(QDateTime(date)), graph);
            } else {
                qDebug() << "Unsupported bday format:" << field.fieldValue[0];
            }
        }

        else {
            qDebug() << "Unsupported VCard field" << field.fieldName;
        }
    }
}

QString CDTpStorage::ensureContactAffiliationToBuilder(CDTpQueryBuilder &builder,
        const QString &imContact,
        const QString &graph,
        const Tp::ContactInfoField &field,
        QHash<QString, QString> &affiliations) const
{
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

    if (!affiliations.contains(type)) {
        const QString affiliation = builder.uniquify("_:affiliation");
        builder.createResource(affiliation, "nco:Affiliation", graph);
        builder.insertProperty(affiliation, "rdfs:label", literal(type), graph);
        builder.insertProperty(imContact, "nco:hasAffiliation", affiliation, graph);
        affiliations.insert(type, affiliation);
    }

    return affiliations[type];
}

void CDTpStorage::addRemoveContactsToBuilder(CDTpQueryBuilder &builder,
        CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contacts) const
{
    QStringList imAddresses;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        imAddresses << literalIMAddress(contactWrapper);
    }

    const QString imAccount = literalIMAccount(accountWrapper);
    const QString imAddress = builder.uniquify("?imAddress");

    /* Bind imAddress to all those contacts */
    builder.appendRawSelection(QString(QLatin1String(
            "%1 a nco:IMAddress.\n"
            "FILTER (%1 IN (%2)).")).arg(imAddress).arg(imAddresses.join(",")));

    addRemoveContactToBuilder(builder, imAddress);
    builder.deleteResource(imAddress);
    builder.deleteProperty(imAccount, "nco:hasIMContact", imAddress);
}

void CDTpStorage::addRemoveContactToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress) const
{
    /* imAddress is used as graph for ContactInfo fields, so we can know
     * which properties to delete from the nco:PersonContact */
    const QString graph = imAddress;

    /* We create a temporary builder that will get merged into builder. We do
     * this because the WHERE block must be wrapper into an OPTIONAL */
    CDTpQueryBuilder tmpBuilder;

    /* Update the nco:PersonContact linked to imAddress if it doesn't have our
     * generator */
    tmpBuilder.appendRawSelection(QString(QLatin1String(
            "?imContact_update a nco:PersonContact.\n"
            "?imContact_update nco:hasAffiliation ?affiliation_update.\n"
            "?affiliation_update nco:hasIMAddress %1.\n"
            "OPTIONAL { ?imContact_update nie:generator ?generator }.\n"
            "FILTER(!bound(?generator) || ?generator != %2)."))
            .arg(imAddress).arg(defaultGenerator));

    addRemoveContactInfoToBuilder(tmpBuilder, "?imContact_update", graph);
    tmpBuilder.deleteResource("?affiliation_update");
    tmpBuilder.updateProperty("?imContact_update", "nie:contentLastModified", literalTimeStamp());

    builder.mergeWithOptional(tmpBuilder);
    tmpBuilder.clear();

    /* Remove the nco:PersonContact linked to imAddress if it has our generator */
    tmpBuilder.appendRawSelection(QString(QLatin1String(
            "?imContact_delete a nco:PersonContact.\n"
            "?imContact_delete nco:hasAffiliation ?affiliation_delete.\n"
            "?affiliation_delete nco:hasIMAddress %1.\n"
            "?imContact_delete nie:generator %2."))
            .arg(imAddress).arg(defaultGenerator));

    addRemoveContactInfoToBuilder(tmpBuilder, "?imContact_delete", graph);
    tmpBuilder.deleteResource("?affiliation_delete");
    tmpBuilder.deleteResource("?imContact_delete");

    builder.mergeWithOptional(tmpBuilder);
}

void CDTpStorage::addRemoveContactInfoToBuilder(CDTpQueryBuilder &builder,
        const QString &imContact,
        const QString &graph) const
{
    builder.deletePropertyWithGraph(imContact, "nco:birthDate", graph);
    builder.deletePropertyWithGraph(imContact, "nco:note", graph);

    /* Remove affiliation and its sub resources */
    const QString affiliation = builder.deletePropertyWithGraph(imContact, "nco:hasAffiliation", graph);
    builder.deletePropertyAndLinkedResource(affiliation, "nco:hasPhoneNumber");
    builder.deletePropertyAndLinkedResource(affiliation, "nco:hasPostalAddress");
    builder.deletePropertyAndLinkedResource(affiliation, "nco:emailAddress");
    builder.deleteResource(affiliation);
}

void CDTpStorage::addSetContactsUnknownToBuilder(CDTpQueryBuilder &builder,
        CDTpAccountPtr accountWrapper) const
{
    Tp::AccountPtr account = accountWrapper->account();

    qDebug() << "Setting all contacts to UNKNOWN:" << account->objectPath();

    /* Bind imAddress to all account's contacts, and imContact to their
     * respective nco:PersonContact. Except for the self contact, because we
     * know that self contact has presence Offline, and that's already set in
     * SyncAccount(). */
    const QString imAddress = builder.uniquify("?imAddress");
    const QString imContact = builder.uniquify("?imContact");
    builder.appendRawSelection(QString(QLatin1String(
            "%1 a nco:IMAccount.\n"
            "%1 nco:hasIMContact %2.\n"
            "%3 a nco:PersonContact.\n"
            "%3 nco:hasAffiliation [ nco:hasIMAddress %2 ].\n"
            "FILTER(%2 != %4)."))
            .arg(literalIMAccount(accountWrapper)).arg(imAddress).arg(imContact)
            .arg(literalIMAddress(accountWrapper)));

    // FIXME: we could use addPresenceToBuilder() somehow
    builder.updateProperty(imAddress, "nco:imPresence", "nco:presence-status-unknown");
    builder.updateProperty(imAddress, "nco:presenceLastModified", literalTimeStamp());
    builder.updateProperty(imContact, "nie:contentLastModified", literalTimeStamp());

    // Update capabilities of all contacts, since we don't know them anymore,
    // reset them to the account's caps.
    addCapabilitiesToBuilder(builder, imAddress,
            accountWrapper->account()->capabilities());
}

QString CDTpStorage::presenceType(Tp::ConnectionPresenceType presenceType) const
{
    switch (presenceType) {
    case Tp::ConnectionPresenceTypeUnset:
        return QString("nco:presence-status-unknown");
    case Tp::ConnectionPresenceTypeOffline:
        return QString("nco:presence-status-offline");
    case Tp::ConnectionPresenceTypeAvailable:
        return QString("nco:presence-status-available");
    case Tp::ConnectionPresenceTypeAway:
        return QString("nco:presence-status-away");
    case Tp::ConnectionPresenceTypeExtendedAway:
        return QString("nco:presence-status-extended-away");
    case Tp::ConnectionPresenceTypeHidden:
        return QString("nco:presence-status-hidden");
    case Tp::ConnectionPresenceTypeBusy:
        return QString("nco:presence-status-busy");
    case Tp::ConnectionPresenceTypeUnknown:
        return QString("nco:presence-status-unknown");
    case Tp::ConnectionPresenceTypeError:
        return QString("nco:presence-status-error");
    default:
        break;
    }

    qWarning() << "Unknown telepathy presence status" << presenceType;

    return QString("nco:presence-status-error");
}

QString CDTpStorage::presenceState(Tp::Contact::PresenceState presenceState) const
{
    switch (presenceState) {
    case Tp::Contact::PresenceStateNo:
        return QString("nco:predefined-auth-status-no");
    case Tp::Contact::PresenceStateAsk:
        return QString("nco:predefined-auth-status-requested");
    case Tp::Contact::PresenceStateYes:
        return QString("nco:predefined-auth-status-yes");
    }

    qWarning() << "Unknown telepathy presence state:" << presenceState;

    return QString("nco:predefined-auth-status-no");
}

// Copied from cubi/src/literalvalue.cpp
static QString
sparqlEscape(const QString &original)
{
	QString escaped;
	escaped.reserve(original.size());

	for(int i = 0; i < original.size(); ++i) {
		switch(original.at(i).toAscii()) {
			case '\t':
				escaped.append("\\t");
				break;
			case '\n':
				escaped.append("\\n");
				break;
			case '\r':
				escaped.append("\\r");
				break;
			case '"':
				escaped.append("\\\"");
				break;
			case '\\':
				escaped.append("\\\\");
				break;
			default:
				escaped.append(original.at(i));
		}
	}

	return escaped;
}

QString CDTpStorage::literal(const QString &str) const
{
    static const QString stringTemplate = QString::fromLatin1("\"%1\"");
    return stringTemplate.arg(sparqlEscape(str));
}

// Copied from cubi/src/literalvalue.cpp
QString CDTpStorage::literal(const QDateTime &dateTimeValue) const
{
    static const QString utcTimeTemplate = QString::fromLatin1("%1Z");

    // This is a workaround for http://bugreports.qt.nokia.com/browse/QTBUG-9698
    return literal(dateTimeValue.timeSpec() == Qt::UTC ?
            utcTimeTemplate.arg(dateTimeValue.toString(Qt::ISODate))
            : dateTimeValue.toString(Qt::ISODate));
}

QString CDTpStorage::literalTimeStamp() const
{
    return literal(QDateTime::currentDateTime());
}

QString CDTpStorage::literalIMAddress(const QString &accountPath, const QString &contactId) const
{
    return QString("<telepathy:%1!%2>").arg(accountPath).arg(contactId);
}

QString CDTpStorage::literalIMAddress(const CDTpContactPtr &contactWrapper) const
{
    const QString accountPath = contactWrapper->accountWrapper()->account()->objectPath();
    const QString contactId = contactWrapper->contact()->id();
    return literalIMAddress(accountPath, contactId);
}

QString CDTpStorage::literalIMAddress(const CDTpAccountPtr &accountWrapper) const
{
    const QString accountPath = accountWrapper->account()->objectPath();
    const QString accountId = accountWrapper->account()->normalizedName();
    return literalIMAddress(accountPath, accountId);
}

QString CDTpStorage::literalIMAccount(const CDTpAccountPtr &accountWrapper) const
{
    return QString("<telepathy:%1>").arg(accountWrapper->account()->objectPath());
}

QString CDTpStorage::literalContactInfo(const Tp::ContactInfoField &field, int i) const
{
    if (i >= field.fieldValue.count()) {
        return literal(QString());
    }

    return literal(field.fieldValue[i]);
}

/* --- CDTpStorageSyncOperations --- */

CDTpStorageSyncOperations::CDTpStorageSyncOperations() : active(false),
    nPendingOperations(0), nContactsAdded(0), nContactsRemoved(0)
{
}

