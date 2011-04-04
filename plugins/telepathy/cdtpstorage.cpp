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
#include <qtcontacts-tracker/phoneutils.h>

#include "cdtpstorage.h"
#include "debug.h"

using namespace Contactsd;

static const QString privateGraph = QString::fromLatin1("<urn:uuid:679293d4-60f0-49c7-8d63-f1528fe31f66>");
static const QString defaultGenerator = QString::fromLatin1("\"telepathy\"");
static const QString filterInSeparator = QString::fromLatin1(",");
static const QString defaultContactMe = QString::fromLatin1("nco:default-contact-me");
static const QString imAddressVar = QString::fromLatin1("?imAddress");
static const QString imContactVar = QString::fromLatin1("?imContact");
static const QString imAccountVar = QString::fromLatin1("?imAccount");

CDTpStorage::CDTpStorage(QObject *parent) : QObject(parent), mUpdateRunning(false)
{
    mUpdateTimer.setInterval(0);
    mUpdateTimer.setSingleShot(true);
    connect(&mUpdateTimer, SIGNAL(timeout()), SLOT(onUpdateQueueTimeout()));
}

CDTpStorage::~CDTpStorage()
{
}

void CDTpStorage::syncAccounts(const QList<CDTpAccountPtr> &accounts)
{
    /* Remove IMAccount that does not exist anymore */
    CDTpQueryBuilder builder("SyncAccounts - purge imAccount");
    static const QString tmpl = QString::fromLatin1("?imAccount a nco:IMAccount.");
    QString selection = tmpl;
    if (!accounts.isEmpty()) {
        static const QString tmpl = QString::fromLatin1("\nFILTER (str(?imAccount) NOT IN (%1)).");
        selection += tmpl.arg(literalIMAccountList(accounts));
    }
    builder.appendRawSelection(selection);
    builder.deleteResource(imAccountVar);

    /* Sync accounts and their contacts */
    if (!accounts.isEmpty()) {
        builder.appendRawQuery(createAccountsBuilder(accounts));

        // Split accounts that have their roster, and those who don't
        QList<CDTpAccountPtr> rosterAccounts;
        QList<CDTpAccountPtr> noRosterAccounts;
        Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
            if (accountWrapper->hasRoster()) {
                rosterAccounts << accountWrapper;
            } else {
                noRosterAccounts << accountWrapper;
            }
        }

        // Sync contacts
        if (!rosterAccounts.isEmpty()) {
            builder.appendRawQuery(syncRosterAccountsContactsBuilder(rosterAccounts));
        }

        if (!noRosterAccounts.isEmpty()) {
            builder.appendRawQuery(syncNoRosterAccountsContactsBuilder(noRosterAccounts));
        }
    }

    /* Purge IMAddresses not bound from an account anymore, this include the
     * self IMAddress and the default-contact-me as well */
    builder.appendRawQuery(purgeContactsBuilder());

    new CDTpSparqlQuery(builder, this);
}

void CDTpStorage::createAccount(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder("CreateAccount");

    /* Create account */
    QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
    builder.appendRawQuery(createAccountsBuilder(accounts));

    /* if account has no contacts, we are done */
    if (accountWrapper->contacts().isEmpty()) {
        new CDTpSparqlQuery(builder, this);
        return;
    }

    /* Create account's contacts */
    builder.appendRawQuery(createContactsBuilder(accountWrapper->contacts()));

    /* Update timestamp on all nco:PersonContact bound to this account */
    static const QString tmpl = QString::fromLatin1(
            "%1 nco:hasIMContact ?imAddress.\n"
            "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].");
    CDTpQueryBuilder subBuilder("CreateAccount - update timestamp");
    subBuilder.appendRawSelection(tmpl.arg(literalIMAccount(accountWrapper)));
    subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
    builder.appendRawQuery(subBuilder);

    /* Notify import progress */
    Q_EMIT syncStarted(accountWrapper);
    CDTpAccountsSparqlQuery *query = new CDTpAccountsSparqlQuery(accountWrapper, builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSyncOperationEnded(CDTpSparqlQuery *)));
}

void CDTpStorage::updateAccount(CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes)
{
    debug() << "Update account" << literalIMAddress(accountWrapper);

    CDTpQueryBuilder builder("UpdateAccount");
    builder.insertProperty(defaultContactMe, "nie:contentLastModified", literalTimeStamp());
    addAccountChangesToBuilder(builder, accountWrapper, changes);

    new CDTpSparqlQuery(builder, this);
}

void CDTpStorage::removeAccount(CDTpAccountPtr accountWrapper)
{
    const QString imAccount = literalIMAccount(accountWrapper);
    debug() << "Remove account" << imAccount;

    /* Remove account */
    CDTpQueryBuilder builder = CDTpQueryBuilder("RemoveAccount");
    builder.deleteResource(literalIMAccount(accountWrapper));

    /* Purge IMAddresses not bound from an account anymore.
     * This will at least remove the IMAddress of the self contact and update
     * default-contact-me */
    builder.appendRawQuery(purgeContactsBuilder());

    new CDTpSparqlQuery(builder, this);
}

CDTpQueryBuilder CDTpStorage::createAccountsBuilder(const QList<CDTpAccountPtr> &accounts) const
{
    CDTpQueryBuilder builder("CreateAccounts");

    builder.insertProperty(defaultContactMe, "nie:contentLastModified", literalTimeStamp());

    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        Tp::AccountPtr account = accountWrapper->account();
        const QString imAccount = literalIMAccount(accountWrapper);
        const QString imAddress = literalIMAddress(accountWrapper);

        debug() << "Create account" << imAddress;

        // Ensure the IMAccount exists
        builder.createResource(imAccount, "nco:IMAccount", privateGraph);
        builder.insertProperty(imAccount, "nco:imAccountType", literal(account->protocolName()), privateGraph);
        builder.insertProperty(imAccount, "nco:imAccountAddress", imAddress, privateGraph);
        builder.insertProperty(imAccount, "nco:hasIMContact", imAddress, privateGraph);

        // Ensure the self contact has an IMAddress
        builder.createResource(imAddress, "nco:IMAddress", privateGraph);
        builder.insertProperty(imAddress, "nco:imID", literal(account->normalizedName()), privateGraph);
        builder.insertProperty(imAddress, "nco:imProtocol", literal(account->protocolName()), privateGraph);

        // Add all mutable properties
        addAccountChangesToBuilder(builder, accountWrapper, CDTpAccount::All);
    }

    // Ensure the IMAddresses are bound to the default-contact-me via an affiliation
    static const QString tmpl = QString::fromLatin1(
        "INSERT or REPLACE {\n"
        "    GRAPH %1 {\n"
        "        nco:default-contact-me nco:hasAffiliation _:affiliation.\n"
        "        _:affiliation a nco:Affiliation;\n"
        "                      nco:hasIMAddress ?imAddress;\n"
        "                      rdfs:label \"Other\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    ?imAddress a nco:IMAddress.\n"
        "    FILTER (str(?imAddress) IN (%2) &&\n"
        "            NOT EXISTS { nco:default-contact-me nco:hasAffiliation [ nco:hasIMAddress ?imAddress ] })\n"
        "}\n");
    builder.appendRawQuery(tmpl.arg(privateGraph).arg(literalIMAddressList(accounts)));

    return builder;
}

void CDTpStorage::addAccountChangesToBuilder(CDTpQueryBuilder &builder,
        CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes) const
{
    Tp::AccountPtr account = accountWrapper->account();
    const QString imAccount = literalIMAccount(accountWrapper);
    const QString imAddress = literalIMAddress(accountWrapper);

    if (changes & CDTpAccount::Presence) {
        debug() << "  presence changed";
        addPresenceToBuilder(builder, imAddress, account->currentPresence());
    }
    if (changes & CDTpAccount::Avatar) {
        debug() << "  avatar changed";
        addAvatarToBuilder(builder, imAddress, saveAccountAvatar(accountWrapper));
    }
    if (changes & CDTpAccount::Nickname) {
        debug() << "  nickname changed";
        builder.insertProperty(imAddress, "nco:imNickname", literal(account->nickname()),
                privateGraph);
    }
    if (changes & CDTpAccount::DisplayName) {
        debug() << "  display name changed";
        builder.insertProperty(imAccount, "nco:imDisplayName", literal(account->displayName()),
                privateGraph);
    }
    if (changes & CDTpAccount::Enabled) {
        debug() << "  enabled changed";
        builder.insertProperty(imAccount, "nco:imEnabled", literal(account->isEnabled()),
                privateGraph);
    }
}

QString CDTpStorage::saveAccountAvatar(CDTpAccountPtr accountWrapper) const
{
    const Tp::Avatar &avatar = accountWrapper->account()->avatar();

    if (avatar.avatarData.isEmpty()) {
        return QString();
    }

    static const QString tmpl = QString::fromLatin1("%1/.contacts/avatars/%2");
    QString fileName = tmpl.arg(QDir::homePath())
        .arg(QLatin1String(QCryptographicHash::hash(avatar.avatarData, QCryptographicHash::Sha1).toHex()));
    debug() << "Saving account avatar to" << fileName;

    QFile avatarFile(fileName);
    if (!avatarFile.open(QIODevice::WriteOnly)) {
        warning() << "Unable to save account avatar: error opening avatar "
            "file" << fileName << "for writing";
        return QString();
    }
    avatarFile.write(avatar.avatarData);
    avatarFile.close();

    return fileName;
}

// This is called when account goes online/offline
void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder;

    QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
    if (accountWrapper->hasRoster()) {
        builder = syncRosterAccountsContactsBuilder(accounts, true);
    } else {
        builder = syncNoRosterAccountsContactsBuilder(accounts);
    }

    /* If it is not the first time account gets a roster, or if account has
     * no contacts, execute query without notify import progress */
    if (!accountWrapper->isNewAccount() || accountWrapper->contacts().isEmpty()) {
        new CDTpSparqlQuery(builder, this);
        return;
    }

    /* Notify import progress */
    Q_EMIT syncStarted(accountWrapper);
    CDTpAccountsSparqlQuery *query = new CDTpAccountsSparqlQuery(accountWrapper, builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSyncOperationEnded(CDTpSparqlQuery *)));
}

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contactsAdded,
        const QList<CDTpContactPtr> &contactsRemoved)
{
    CDTpQueryBuilder builder("Contacts Added/Removed");

    if (!contactsAdded.isEmpty()) {
        builder.appendRawQuery(createContactsBuilder(contactsAdded));

        // Update nie:contentLastModified on all nco:PersonContact bound to contacts
        static const QString tmpl = QString::fromLatin1(
                "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
                "FILTER(str(?imAddress) IN (%1)).");
        CDTpQueryBuilder subBuilder("Contacts Added/Removed - update timestamp");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(contactsAdded)));
        subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
        builder.appendRawQuery(subBuilder);
    }

    if (!contactsRemoved.isEmpty()) {
        builder.appendRawQuery(removeContactsBuilder(accountWrapper, contactsRemoved));
    }

    new CDTpSparqlQuery(builder, this);
}

/* Use this only in offline mode - use syncAccountContacts in online mode */
void CDTpStorage::removeAccountContacts(const QString &accountPath, const QStringList &contactIds)
{
    new CDTpSparqlQuery(removeContactsBuilder(accountPath, contactIds), this);
}

void CDTpStorage::updateContact(CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    if (!mUpdateQueue.contains(contactWrapper)) {
        mUpdateQueue.insert(contactWrapper, changes);
    } else {
        mUpdateQueue[contactWrapper] |= changes;
    }

    if (!mUpdateRunning) {
        mUpdateTimer.start();
    }
}

void CDTpStorage::onUpdateQueueTimeout()
{
    debug() << "Update" << mUpdateQueue.count() << "contacts";

    CDTpQueryBuilder builder("UpdateContacts");

    QList<CDTpContactPtr> allContacts;
    QList<CDTpContactPtr> infoContacts;
    QList<CDTpContactPtr> capsContacts;
    QHash<CDTpContactPtr, CDTpContact::Changes>::const_iterator i;
    for (i = mUpdateQueue.constBegin(); i != mUpdateQueue.constEnd(); i++) {
        CDTpContactPtr contactWrapper = i.key();
        CDTpContact::Changes changes = i.value();
        if (!contactWrapper->isVisible()) {
            continue;
        }

        allContacts << contactWrapper;

        // Special case for Capabilities and Information changes
        if (changes & CDTpContact::Capabilities) {
            capsContacts << contactWrapper;
        }
        if (changes & CDTpContact::Information) {
            infoContacts << contactWrapper;
        }

        // Add IMAddress changes
        addContactChangesToBuilder(builder, literalIMAddress(contactWrapper),
                changes, contactWrapper->contact());
    }
    mUpdateQueue.clear();

    if (allContacts.isEmpty()) {
        debug() << "  None needs update";
        return;
    }

    // prepend delete nco:imCapability for contacts who's caps changed
    if (!capsContacts.isEmpty()) {
        static const QString tmpl = QString::fromLatin1(
                "?imAddress nco:imCapability ?v.\n"
                "FILTER(str(?imAddress) IN (%1)).");
        CDTpQueryBuilder subBuilder("UpdateContacts - remove caps");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(capsContacts)));
        subBuilder.deleteProperty(imAddressVar, "nco:imCapability", QLatin1String("?v"));
        builder.prependRawQuery(subBuilder);
    }

    // delete ContactInfo for contacts who's info changed
    if (!infoContacts.isEmpty()) {
        static const QString tmpl = QString::fromLatin1(
                "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
                "FILTER(str(?imAddress) IN (%1)).");
        CDTpQueryBuilder subBuilder("UpdateContacts - remove info");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(infoContacts)));
        addRemoveContactInfoToBuilder(subBuilder, imAddressVar, imContactVar);
        builder.appendRawQuery(subBuilder);
    }

    // Create ContactInfo for each contact who's info changed
    Q_FOREACH (const CDTpContactPtr contactWrapper, infoContacts) {
        builder.appendRawQuery(createContactInfoBuilder(contactWrapper));
    }

    // Update nie:contentLastModified on all nco:PersonContact bound to contacts
    static const QString tmpl = QString::fromLatin1(
            "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
            "FILTER(str(?imAddress) IN (%1)).");
    CDTpQueryBuilder subBuilder("UpdateContacts - update timestamp");
    subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(allContacts)));
    subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
    builder.appendRawQuery(subBuilder);

    // Launch the query
    mUpdateRunning = true;
    CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onUpdateFinished(CDTpSparqlQuery *)));
}

void CDTpStorage::onUpdateFinished(CDTpSparqlQuery *query)
{
    Q_UNUSED(query);

    mUpdateRunning = false;
    if (!mUpdateQueue.isEmpty()) {
        mUpdateTimer.start();
    }
}

CDTpQueryBuilder CDTpStorage::syncNoRosterAccountsContactsBuilder(const QList<CDTpAccountPtr> accounts) const
{
    // Set presence to UNKNOWN for all contacts, except for self contact because
    // its presence is OFFLINE and will be set in updateAccount()
    static const Tp::Presence unknownPresence(Tp::ConnectionPresenceTypeUnknown, QLatin1String("unknown"), QString());
    static const QString tmpl = QString::fromLatin1(
            "?imAccount nco:hasIMContact ?imAddress.\n"
            "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
            "FILTER(str(?imAddress) NOT IN (%1) && str(?imAccount) IN (%2)).");
    CDTpQueryBuilder builder("SyncNoRosterAccountsContacts");
    builder.appendRawSelection(tmpl.arg(literalIMAddressList(accounts)).arg(literalIMAccountList(accounts)));
    addPresenceToBuilder(builder, imAddressVar, unknownPresence);
    builder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());

    // Remove capabilities from all imAddresses
    static const QString tmpl2 = QString::fromLatin1(
            "?imAccount nco:hasIMContact ?imAddress.\n"
            "FILTER(str(?imAccount) IN (%1)).");
    CDTpQueryBuilder subBuilder("SyncNoRosterAccountsContacts - remove caps");
    subBuilder.appendRawSelection(tmpl2.arg(literalIMAccountList(accounts)));
    addRemoveCapabilitiesToBuilder(subBuilder, imAddressVar);
    builder.appendRawQuery(subBuilder);

    // Add capabilities on all contacts for each account
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        static const QString tmpl = QString::fromLatin1(
                "%1 nco:hasIMContact ?imAddress.");
        CDTpQueryBuilder subBuilder("SyncNoRosterAccountsContacts - add caps");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAccount(accountWrapper)));
        addCapabilitiesToBuilder(subBuilder, imAddressVar,
                accountWrapper->account()->capabilities());
        builder.appendRawQuery(subBuilder);
    }

    return builder;
}

CDTpQueryBuilder CDTpStorage::syncRosterAccountsContactsBuilder(const QList<CDTpAccountPtr> &accounts,
        bool purgeContacts) const
{
    QList<CDTpContactPtr> allContacts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        allContacts << accountWrapper->contacts();
    }

    /* Delete the hasIMContact property on IMAccounts (except for self contact)
     * then sync all contacts (that will add back hasIMContact for them). After
     * that we can purge all imAddresses not bound to an IMAccount anymore.
     *
     * Also remove imCapability from imAddresses because it is multi-valued */
    CDTpQueryBuilder builder = CDTpQueryBuilder("SyncRosterAccounts - drop hasIMContact");
    static const QString tmpl = QString::fromLatin1(
            "?imAccount nco:hasIMContact ?imAddress.\n"
            "FILTER(str(?imAddress) NOT IN (%1) && str(?imAccount) IN (%2)).");
    builder.appendRawSelection(tmpl.arg(literalIMAddressList(accounts)).arg(literalIMAccountList(accounts)));
    builder.deleteProperty(imAccountVar, "nco:hasIMContact", imAddressVar);
    addRemoveCapabilitiesToBuilder(builder, imAddressVar);

    /* Delete ContactInfo for those who know it */
    QStringList infoIMAddresses;
    Q_FOREACH (const CDTpContactPtr contactWrapper, allContacts) {
        if (contactWrapper->isInformationKnown()) {
            infoIMAddresses << literalIMAddress(contactWrapper);
        }
    }
    if (!infoIMAddresses.isEmpty()) {
        CDTpQueryBuilder subBuilder("SyncRosterAccounts  - remove known ContactInfo");
        static const QString tmpl = QString::fromLatin1(
                "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
                "FILTER(?imAddress IN (%1))");
        subBuilder.appendRawSelection(tmpl.arg(infoIMAddresses.join(filterInSeparator)));
        addRemoveContactInfoToBuilder(subBuilder, imAddressVar, imContactVar);
        builder.appendRawQuery(subBuilder);
    }

    /* Now create all contacts */
    if (!allContacts.isEmpty()) {
        builder.appendRawQuery(createContactsBuilder(allContacts));

        /* Update timestamp on all nco:PersonContact bound to this account */
        static const QString tmpl = QString::fromLatin1(
                "?imAccount nco:hasIMContact ?imAddress.\n"
                "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
                "FILTER(str(?imAccount) IN (%1)).");
        CDTpQueryBuilder subBuilder("SyncRosterAccounts - update timestamp");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAccountList(accounts)));
        subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
        builder.appendRawQuery(subBuilder);
    }

    /* Purge IMAddresses not bound from an account anymore */
    if (purgeContacts) {
        builder.appendRawQuery(purgeContactsBuilder());
    }

    return builder;
}

CDTpQueryBuilder CDTpStorage::createContactsBuilder(const QList<CDTpContactPtr> &contacts) const
{
    CDTpQueryBuilder builder("CreateContacts");

    // Ensure all imAddress exists and are linked from imAccount
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();

        const QString imAddress = literalIMAddress(contactWrapper);
        builder.createResource(imAddress, "nco:IMAddress", privateGraph);
        builder.insertProperty(imAddress, "nco:imID", literal(contactWrapper->contact()->id()),
                privateGraph);
        builder.insertProperty(imAddress, "nco:imProtocol", literal(accountWrapper->account()->protocolName()),
                privateGraph);

        const QString imAccount = literalIMAccount(accountWrapper);
        builder.insertProperty(imAccount, "nco:hasIMContact", imAddress, privateGraph);

        // Add mutable properties except for ContactInfo
        addContactChangesToBuilder(builder, imAddress, CDTpContact::All,
                contactWrapper->contact());
    }

    // Ensure all imAddresses are bound to a PersonContact
    static const QString tmpl = QString::fromLatin1(
        "INSERT {\n"
        "    GRAPH %1 {\n"
        "        _:contact a nco:PersonContact;\n"
        "                  nie:contentCreated %3;\n"
        "                  nie:contentLastModified %3;\n"
        "                  nie:generator %4.\n"
        "    }\n"
        "    GRAPH %2 {\n"
        "        _:contact nco:hasAffiliation _:affiliation.\n"
        "        _:affiliation a nco:Affiliation;\n"
        "                      nco:hasIMAddress ?imAddress;\n"
        "                      rdfs:label \"Other\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    GRAPH %2 { ?imAddress a nco:IMAddress }.\n"
        "    FILTER (NOT EXISTS { ?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ] })\n"
        "}\n");
    builder.appendRawQuery(tmpl.arg(CDTpQueryBuilder::defaultGraph).arg(privateGraph)
            .arg(literalTimeStamp()).arg(defaultGenerator));

    // Add ContactInfo seperately because we need to bind to the PersonContact
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        builder.appendRawQuery(createContactInfoBuilder(contactWrapper));
    }

    return builder;
}

void CDTpStorage::addContactChangesToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        CDTpContact::Changes changes,
        Tp::ContactPtr contact) const
{
    debug() << "Update contact" << imAddress;

    // Apply changes
    if (changes & CDTpContact::Alias) {
        debug() << "  alias changed";
        builder.insertProperty(imAddress, "nco:imNickname", literal(contact->alias().trimmed()),
                privateGraph);
    }
    if (changes & CDTpContact::Presence) {
        debug() << "  presence changed";
        addPresenceToBuilder(builder, imAddress, contact->presence());
    }
    if (changes & CDTpContact::Capabilities) {
        debug() << "  capabilities changed";
        addCapabilitiesToBuilder(builder, imAddress, contact->capabilities());
    }
    if (changes & CDTpContact::Avatar) {
        debug() << "  avatar changed";
        addAvatarToBuilder(builder, imAddress, contact->avatarData().fileName);
    }
    if (changes & CDTpContact::Authorization) {
        debug() << "  authorization changed";
        builder.insertProperty(imAddress, "nco:imAddressAuthStatusFrom",
                presenceState(contact->subscriptionState()), privateGraph);
        builder.insertProperty(imAddress, "nco:imAddressAuthStatusTo",
                presenceState(contact->publishState()), privateGraph);
    }
}

void CDTpStorage::addPresenceToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const Tp::Presence &presence) const
{
    builder.insertProperty(imAddress, "nco:imPresence", presenceType(presence.type()),
            privateGraph);
    builder.insertProperty(imAddress, "nco:presenceLastModified", literalTimeStamp(),
            privateGraph);
    builder.insertProperty(imAddress, "nco:imStatusMessage", literal(presence.statusMessage()),
            privateGraph);
}

void CDTpStorage::addRemoveCapabilitiesToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress) const
{
    builder.deleteProperty(imAddress, "nco:imCapability");
}

void CDTpStorage::addCapabilitiesToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        Tp::CapabilitiesBase capabilities) const
{
    if (capabilities.textChats()) {
        static const QString value = QString::fromLatin1("nco:im-capability-text-chat");
        builder.insertProperty(imAddress, "nco:imCapability", value, privateGraph);
    }
    if (capabilities.streamedMediaAudioCalls()) {
        static const QString value = QString::fromLatin1("nco:im-capability-audio-calls");
        builder.insertProperty(imAddress, "nco:imCapability", value, privateGraph);
    }
    if (capabilities.streamedMediaVideoCalls()) {
        static const QString value = QString::fromLatin1("nco:im-capability-video-calls");
        builder.insertProperty(imAddress, "nco:imCapability", value, privateGraph);
    }
}

void CDTpStorage::addAvatarToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &fileName) const
{
    if (!fileName.isEmpty()) {
        static const QString tmpl = QString::fromLatin1("<%1>");
        const QString url = QUrl::fromLocalFile(fileName).toString();
        const QString dataObject = tmpl.arg(url);
        builder.createResource(dataObject, "nfo:FileDataObject", privateGraph);
        builder.insertProperty(dataObject, "nie:url", literal(url), privateGraph);
        builder.insertProperty(imAddress, "nco:imAvatar", dataObject, privateGraph);
    }
}

CDTpQueryBuilder CDTpStorage::createContactInfoBuilder(CDTpContactPtr contactWrapper) const
{
    if (!contactWrapper->isInformationKnown()) {
        return CDTpQueryBuilder();
    }

    /* Use the imAddress as graph for ContactInfo fields, so we can easilly
     * know from which contact it comes from */
    const QString graph = literalIMAddress(contactWrapper);

    Tp::ContactInfoFieldList listContactInfo = contactWrapper->contact()->infoFields().allFields();
    if (listContactInfo.count() == 0) {
        debug() << "No contact info present";
        return CDTpQueryBuilder();
    }

    static const QString tmpl = QString::fromLatin1("?imContact nco:hasAffiliation [ nco:hasIMAddress %1 ].");
    CDTpQueryBuilder builder("CreateContactInfo");
    builder.appendRawSelection(tmpl.arg(literalIMAddress(contactWrapper)));

    QHash<QString, QString> affiliations;
    Q_FOREACH (const Tp::ContactInfoField &field, listContactInfo) {
        if (field.fieldValue.count() == 0) {
            continue;
        }

        /* FIXME:
         *  - Do we care about "fn" and "nickname" ?
         *  - How do we write affiliation for "org" ?
         */

        if (!field.fieldName.compare(QLatin1String("tel"))) {
            const QString voicePhoneNumber = literalContactInfo(field, 0);

            /* FIXME: Could change nco:PhoneNumber depending the the TYPE */
            CDTpQueryBuilder subBuilder("Insert Anon PhoneNumber");
            static const QString tmplQuery = QString::fromLatin1(
                    "FILTER(NOT EXISTS { ?resource a nco:PhoneNumber ; nco:phoneNumber %1 })");
            const QString phoneVar = subBuilder.uniquify("_:v");
            subBuilder.createResource(phoneVar, "nco:PhoneNumber");
            subBuilder.insertProperty(phoneVar, "nco:phoneNumber", voicePhoneNumber);
            subBuilder.insertProperty(phoneVar, "maemo:localPhoneNumber",
                    literal(qctMakeLocalPhoneNumber(field.fieldValue[0])));
            subBuilder.appendRawSelection(tmplQuery.arg(voicePhoneNumber));
            builder.prependRawQuery(subBuilder);

            /* save phone details to imContact */
            QString var = builder.uniquify("?phoneResource");
            static const QString tmplPhoneResource = QString::fromLatin1(
                "%1 nco:phoneNumber %2.");
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContactVar, graph, field, affiliations);
            builder.appendRawSelectionInsert(tmplPhoneResource.arg(var).arg(voicePhoneNumber));
            builder.insertProperty(affiliation, "nco:hasPhoneNumber", var);
        }

        else if (!field.fieldName.compare(QLatin1String("email"))) {
            // FIXME: Should normalize email address?
            static const QString tmpl = QString::fromLatin1("<mailto:%1>");
            const QString emailAddress = tmpl.arg(field.fieldValue[0]);
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContactVar, graph, field, affiliations);
            builder.createResource(emailAddress, "nco:EmailAddress");
            builder.insertProperty(emailAddress, "nco:emailAddress", literalContactInfo(field, 0));
            builder.insertProperty(affiliation, "nco:hasEmailAddress", emailAddress);
        }

        else if (!field.fieldName.compare(QLatin1String("adr"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContactVar, graph, field, affiliations);
            const QString postalAddress = builder.uniquify("_:address");
            builder.createResource(postalAddress, "nco:PostalAddress");
            builder.insertProperty(postalAddress, "nco:pobox",           literalContactInfo(field, 0));
            builder.insertProperty(postalAddress, "nco:extendedAddress", literalContactInfo(field, 1));
            builder.insertProperty(postalAddress, "nco:streetAddress",   literalContactInfo(field, 2));
            builder.insertProperty(postalAddress, "nco:locality",        literalContactInfo(field, 3));
            builder.insertProperty(postalAddress, "nco:region",          literalContactInfo(field, 4));
            builder.insertProperty(postalAddress, "nco:postalcode",      literalContactInfo(field, 5));
            builder.insertProperty(postalAddress, "nco:country",         literalContactInfo(field, 6));
            builder.insertProperty(affiliation, "nco:hasPostalAddress", postalAddress);
        }

        else if (!field.fieldName.compare(QLatin1String("url"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContactVar, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:url", literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("title"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContactVar, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:title", literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("role"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContactVar, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:role", literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("note")) || !field.fieldName.compare(QLatin1String("desc"))) {
            builder.insertProperty(imContactVar, "nco:note", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare(QLatin1String("bday"))) {
            /* Tracker will reject anything not [-]CCYY-MM-DDThh:mm:ss[Z|(+|-)hh:mm]
             * VCard spec allows only ISO 8601, but most IM clients allows
             * any string. */
            /* FIXME: support more date format for compatibility */
            QDate date = QDate::fromString(field.fieldValue[0], QLatin1String("yyyy-MM-dd"));
            if (!date.isValid()) {
                date = QDate::fromString(field.fieldValue[0], QLatin1String("yyyyMMdd"));
            }

            if (date.isValid()) {
                builder.insertProperty(imContactVar, "nco:birthDate", literal(QDateTime(date)), graph);
            } else {
                debug() << "Unsupported bday format:" << field.fieldValue[0];
            }
        }

        else if (!field.fieldName.compare(QLatin1String("x-gender"))) {
            if (field.fieldValue[0] == QLatin1String("male")) {
                static const QString maleIri = QLatin1String("nco:gender-male");
                builder.insertProperty(imContactVar, "nco:gender", maleIri, graph);
            } else if (field.fieldValue[0] == QLatin1String("female")) {
                static const QString femaleIri = QLatin1String("nco:gender-female");
                builder.insertProperty(imContactVar, "nco:gender", femaleIri, graph);
            } else {
                debug() << "Unsupported gender:" << field.fieldValue[0];
            }
        }

        else {
            debug() << "Unsupported VCard field" << field.fieldName;
        }
    }

    return builder;
}

QString CDTpStorage::ensureContactAffiliationToBuilder(CDTpQueryBuilder &builder,
        const QString &imContact,
        const QString &graph,
        const Tp::ContactInfoField &field,
        QHash<QString, QString> &affiliations) const
{
    static QHash<QString, QString> knownTypes;
    if (knownTypes.isEmpty()) {
        knownTypes.insert (QLatin1String("work"), QLatin1String("Work"));
        knownTypes.insert (QLatin1String("home"), QLatin1String("Home"));
    }

    QString type = QLatin1String("Other");
    Q_FOREACH (const QString &parameter, field.parameters) {
        if (!parameter.startsWith(QLatin1String("type="))) {
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
        builder.createResource(affiliation, "nco:Affiliation");
        builder.insertProperty(affiliation, "rdfs:label", literal(type));
        builder.insertProperty(imContact, "nco:hasAffiliation", affiliation, graph);
        affiliations.insert(type, affiliation);
    }

    return affiliations[type];
}

CDTpQueryBuilder CDTpStorage::removeContactsBuilder(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contacts) const
{
    QStringList contactIds;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        contactIds << contactWrapper->contact()->id();
    }

    return removeContactsBuilder(accountWrapper->account()->objectPath(), contactIds);
}

CDTpQueryBuilder CDTpStorage::removeContactsBuilder(const QString &accountPath,
        const QStringList &contactIds) const
{
    // delete all nco:hasIMContact link from the nco:IMAccount
    CDTpQueryBuilder builder("RemoveContacts");
    const QString imAccount = literalIMAccount(accountPath);
    Q_FOREACH (const QString &contactId, contactIds) {
        builder.deleteProperty(imAccount, "nco:hasIMContact", literalIMAddress(accountPath, contactId));
    }

    // Now purge contacts not linked from an IMAccount
    builder.appendRawQuery(purgeContactsBuilder());

    return builder;
}

CDTpQueryBuilder CDTpStorage::purgeContactsBuilder() const
{
    /* Purge nco:IMAddress not bound from an nco:IMAccount */

    /* Step 1 - Clean nco:PersonContact from all info imported from the imAddress.
     * Note: We don't delete the affiliation because it could contain other
     * info (See NB#239973) */
    static const QString affiliationVar = QString::fromLatin1("?affiliation");
    static const QString selection = QString::fromLatin1(
            "GRAPH %1 { ?imAddress a nco:IMAddress }.\n"
            "?imContact nco:hasAffiliation ?affiliation.\n"
            "?affiliation nco:hasIMAddress ?imAddress.\n"
            "FILTER(NOT EXISTS { ?imAccount nco:hasIMContact ?imAddress }).")
            .arg(privateGraph);
    CDTpQueryBuilder builder("PurgeContacts - step 1");
    builder.appendRawSelection(selection);
    builder.deleteProperty(imContactVar, "nie:contentLastModified");
    builder.deleteProperty(affiliationVar, "nco:hasIMAddress", imAddressVar);
    builder.deleteResource(imAddressVar);
    addRemoveContactInfoToBuilder(builder, imAddressVar, imContactVar);

    /* Step 2 - Purge nco:PersonContact with generator "telepathy" but with no
     * nco:IMAddress bound anymore */
    CDTpQueryBuilder subBuilder("PurgeContacts - step 2");
    static const QString tmpl2 = QString::fromLatin1(
            "?imContact nie:generator %1.\n"
            "FILTER(NOT EXISTS { ?imContact nco:hasAffiliation [nco:hasIMAddress ?v] })")
            .arg(defaultGenerator);
    subBuilder.appendRawSelection(tmpl2);
    subBuilder.deleteResource(imContactVar);
    builder.appendRawQuery(subBuilder);

    /* Step 3 - Add back nie:contentLastModified for nco:PersonContact missing one */
    subBuilder = CDTpQueryBuilder("PurgeContacts - step 3");
    static const QString tmpl3 = QString::fromLatin1(
            "?imContact a nco:PersonContact.\n"
            "FILTER(NOT EXISTS { ?imContact nie:contentLastModified ?v }).");
    subBuilder.appendRawSelection(tmpl3);
    subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
    builder.appendRawQuery(subBuilder);

    return builder;
}

void CDTpStorage::addRemoveContactInfoToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &imContact) const
{
    /* imAddress is used as graph for properties on the imContact */
    const QString graph = imAddress;

    /* Remove all triples on imContact and in graph. All sub-resources will be
     * GCed by qct sometimes */
    builder.deletePropertyWithGraph(imContact, "nco:birthDate", graph);
    builder.deletePropertyWithGraph(imContact, "nco:note", graph);
    builder.deletePropertyWithGraph(imContact, "nco:hasAffiliation", graph);
}

void CDTpStorage::onSyncOperationEnded(CDTpSparqlQuery *query)
{
    CDTpAccountsSparqlQuery *accountsQuery = qobject_cast<CDTpAccountsSparqlQuery*>(query);
    QList<CDTpAccountPtr> accounts = accountsQuery->accounts();

    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        Q_EMIT syncEnded(accountWrapper, accountWrapper->contacts().count(), 0);
    }
}

QString CDTpStorage::presenceType(Tp::ConnectionPresenceType presenceType) const
{
    static const QString statusUnknown = QString::fromLatin1("nco:presence-status-unknown");
    static const QString statusOffline = QString::fromLatin1("nco:presence-status-offline");
    static const QString statusAvailable = QString::fromLatin1("nco:presence-status-available");
    static const QString statusAway = QString::fromLatin1("nco:presence-status-away");
    static const QString statusExtendedAway = QString::fromLatin1("nco:presence-status-extended-away");
    static const QString statusHidden = QString::fromLatin1("nco:presence-status-hidden");
    static const QString statusBusy = QString::fromLatin1("nco:presence-status-busy");
    static const QString statusError = QString::fromLatin1("nco:presence-status-error");

    switch (presenceType) {
    case Tp::ConnectionPresenceTypeUnset:
        return statusUnknown;
    case Tp::ConnectionPresenceTypeOffline:
        return statusOffline;
    case Tp::ConnectionPresenceTypeAvailable:
        return statusAvailable;
    case Tp::ConnectionPresenceTypeAway:
        return statusAway;
    case Tp::ConnectionPresenceTypeExtendedAway:
        return statusExtendedAway;
    case Tp::ConnectionPresenceTypeHidden:
        return statusHidden;
    case Tp::ConnectionPresenceTypeBusy:
        return statusBusy;
    case Tp::ConnectionPresenceTypeUnknown:
        return statusUnknown;
    case Tp::ConnectionPresenceTypeError:
        return statusError;
    default:
        break;
    }

    warning() << "Unknown telepathy presence status" << presenceType;

    return statusError;
}

QString CDTpStorage::presenceState(Tp::Contact::PresenceState presenceState) const
{
    static const QString statusNo = QString::fromLatin1("nco:predefined-auth-status-no");
    static const QString statusRequested = QString::fromLatin1("nco:predefined-auth-status-requested");
    static const QString statusYes = QString::fromLatin1("nco:predefined-auth-status-yes");

    switch (presenceState) {
    case Tp::Contact::PresenceStateNo:
        return statusNo;
    case Tp::Contact::PresenceStateAsk:
        return statusRequested;
    case Tp::Contact::PresenceStateYes:
        return statusYes;
    }

    warning() << "Unknown telepathy presence state:" << presenceState;

    return statusNo;
}

// Copied from cubi/src/literalvalue.cpp
static QString
sparqlEscape(const QString &original)
{
	QString escaped;
	escaped.reserve(original.size());

	for(int i = 0; i < original.size(); ++i) {
		switch(original.at(i).toLatin1()) {
			case '\t':
				escaped.append(QLatin1String("\\t"));
				break;
			case '\n':
				escaped.append(QLatin1String("\\n"));
				break;
			case '\r':
				escaped.append(QLatin1String("\\r"));
				break;
			case '"':
				escaped.append(QLatin1String("\\\""));
				break;
			case '\\':
				escaped.append(QLatin1String("\\\\"));
				break;
			default:
				escaped.append(original.at(i));
		}
	}

	return escaped;
}

QString CDTpStorage::literal(const QString &str) const
{
    static const QString tmpl = QString::fromLatin1("\"%1\"");
    return tmpl.arg(sparqlEscape(str));
}

QString CDTpStorage::literal(bool value) const
{
    static const QString trueString = QString::fromLatin1("true");
    static const QString falseString = QString::fromLatin1("false");
    return value ? trueString : falseString;
}

// Copied from cubi/src/literalvalue.cpp
QString CDTpStorage::literal(const QDateTime &dateTimeValue) const
{
    static const QString tmpl = QString::fromLatin1("%1Z");

    // This is a workaround for http://bugreports.qt.nokia.com/browse/QTBUG-9698
    return literal(dateTimeValue.timeSpec() == Qt::UTC ?
            tmpl.arg(dateTimeValue.toString(Qt::ISODate))
            : dateTimeValue.toString(Qt::ISODate));
}

QString CDTpStorage::literalTimeStamp() const
{
    return literal(QDateTime::currentDateTime());
}

QString CDTpStorage::literalIMAddress(const QString &accountPath, const QString &contactId, bool resource) const
{
    // FIXME: we can't use QUrl::toPercentEncoding() here because the string
    // will be used in some QString::arg().
    static const QString tmpl1 = QString::fromLatin1("<telepathy:%1!%2>");
    static const QString tmpl2 = QString::fromLatin1("\"telepathy:%1!%2\"");
    return (resource ? tmpl1 : tmpl2).arg(accountPath).arg(QString(contactId).remove(QLatin1Char('<')).remove(QLatin1Char('>')));
}

QString CDTpStorage::literalIMAddress(const CDTpContactPtr &contactWrapper, bool resource) const
{
    const QString accountPath = contactWrapper->accountWrapper()->account()->objectPath();
    const QString contactId = contactWrapper->contact()->id();
    return literalIMAddress(accountPath, contactId, resource);
}

QString CDTpStorage::literalIMAddress(const CDTpAccountPtr &accountWrapper, bool resource) const
{
    static const QString tmpl1 = QString::fromLatin1("<telepathy:%1!self>");
    static const QString tmpl2 = QString::fromLatin1("\"telepathy:%1!self\"");
    return (resource ? tmpl1 : tmpl2).arg(accountWrapper->account()->objectPath());
}

QString CDTpStorage::literalIMAddressList(const QList<CDTpContactPtr> &contacts) const
{
    QStringList imAddresses;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        imAddresses << literalIMAddress(contactWrapper, false);
    }
    return imAddresses.join(filterInSeparator);
}

QString CDTpStorage::literalIMAddressList(const QList<CDTpAccountPtr> &accounts) const
{
    QStringList imAddresses;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        imAddresses << literalIMAddress(accountWrapper, false);
    }
    return imAddresses.join(filterInSeparator);
}

QString CDTpStorage::literalIMAccount(const QString &accountPath, bool resource) const
{
    static const QString tmpl1 = QString::fromLatin1("<telepathy:%1>");
    static const QString tmpl2 = QString::fromLatin1("\"telepathy:%1\"");
    return (resource ? tmpl1 : tmpl2).arg(accountPath);
}

QString CDTpStorage::literalIMAccount(const CDTpAccountPtr &accountWrapper, bool resource) const
{
    return literalIMAccount(accountWrapper->account()->objectPath(), resource);
}

QString CDTpStorage::literalIMAccountList(const QList<CDTpAccountPtr> &accounts) const
{
    QStringList imAccounts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        imAccounts << literalIMAccount(accountWrapper, false);
    }
    return imAccounts.join(filterInSeparator);
}

QString CDTpStorage::literalContactInfo(const Tp::ContactInfoField &field, int i) const
{
    if (i >= field.fieldValue.count()) {
        return literal(QString());
    }

    return literal(field.fieldValue[i]);
}

