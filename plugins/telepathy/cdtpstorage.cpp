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

static const Value privateGraph = ResourceValue(QString::fromLatin1("urn:uuid:679293d4-60f0-49c7-8d63-f1528fe31f66"));
static const QString defaultGenerator = QString::fromLatin1("\"telepathy\"");
static const Value defaultContactMe = ResourceValue(QString::fromLatin1("nco:default-contact-me"), ResourceValue::PrefixedName);
static const Value imAddressVar = Variable(QString::fromLatin1("imAddress"));
static const Value imContactVar = Variable(QString::fromLatin1("imContact"));
static const Value imAccountVar = Variable(QString::fromLatin1("imAccount"));

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
    static const QString tmpl = QString::fromLatin1("?_imAccount a nco:IMAccount.");
    QString selection = tmpl;
    if (!accounts.isEmpty()) {
        static const QString tmpl = QString::fromLatin1("\nFILTER (str(?_imAccount) NOT IN %1).");
        selection += tmpl.arg(literalIMAccountList(accounts).sparql());
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
            "%1 nco:hasIMContact ?_imAddress.\n"
            "?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ].");
    CDTpQueryBuilder subBuilder("CreateAccount - update timestamp");
    subBuilder.appendRawSelection(tmpl.arg(literalIMAccount(accountWrapper).sparql()));
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
    debug() << "Update account" << literalIMAddress(accountWrapper).sparql();

    CDTpQueryBuilder builder("UpdateAccount");
    builder.insertProperty(defaultContactMe, "nie:contentLastModified", literalTimeStamp());
    addAccountChangesToBuilder(builder, accountWrapper, changes);

    new CDTpSparqlQuery(builder, this);
}

void CDTpStorage::removeAccount(CDTpAccountPtr accountWrapper)
{
    const Value imAccount = literalIMAccount(accountWrapper);
    debug() << "Remove account" << imAccount.sparql();

    /* Remove account */
    CDTpQueryBuilder builder = CDTpQueryBuilder("RemoveAccount");
    builder.deleteResource(imAccount);

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
        const Value imAccount = literalIMAccount(accountWrapper);
        const Value imAddress = literalIMAddress(accountWrapper);

        debug() << "Create account" << imAddress.sparql();

        // Ensure the IMAccount exists
        builder.createResource(imAccount, "nco:IMAccount", privateGraph);
        builder.insertProperty(imAccount, "nco:imAccountType", LiteralValue(account->protocolName()), privateGraph);
        builder.insertProperty(imAccount, "nco:imAccountAddress", imAddress, privateGraph);
        builder.insertProperty(imAccount, "nco:hasIMContact", imAddress, privateGraph);

        // Ensure the self contact has an IMAddress
        builder.createResource(imAddress, "nco:IMAddress", privateGraph);
        builder.insertProperty(imAddress, "nco:imID", LiteralValue(account->normalizedName()), privateGraph);
        builder.insertProperty(imAddress, "nco:imProtocol", LiteralValue(account->protocolName()), privateGraph);

        // Add all mutable properties
        addAccountChangesToBuilder(builder, accountWrapper, CDTpAccount::All);
    }

    // Ensure the IMAddresses are bound to the default-contact-me via an affiliation
    static const QString tmpl = QString::fromLatin1(
        "INSERT or REPLACE {\n"
        "    GRAPH %1 {\n"
        "        nco:default-contact-me nco:hasAffiliation _:affiliation.\n"
        "        _:affiliation a nco:Affiliation;\n"
        "                      nco:hasIMAddress ?_imAddress;\n"
        "                      rdfs:label \"Other\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    ?_imAddress a nco:IMAddress.\n"
        "    FILTER (str(?_imAddress) IN %2 &&\n"
        "            NOT EXISTS { nco:default-contact-me nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ] })\n"
        "}\n");
    builder.appendRawQuery(tmpl.arg(privateGraph.sparql()).arg(literalIMAddressList(accounts).sparql()));

    return builder;
}

void CDTpStorage::addAccountChangesToBuilder(CDTpQueryBuilder &builder,
        CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes) const
{
    Tp::AccountPtr account = accountWrapper->account();
    const Value imAccount = literalIMAccount(accountWrapper);
    const Value imAddress = literalIMAddress(accountWrapper);

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
        builder.insertProperty(imAddress, "nco:imNickname", LiteralValue(account->nickname()),
                privateGraph);
    }
    if (changes & CDTpAccount::DisplayName) {
        debug() << "  display name changed";
        builder.insertProperty(imAccount, "nco:imDisplayName", LiteralValue(account->displayName()),
                privateGraph);
    }
    if (changes & CDTpAccount::Enabled) {
        debug() << "  enabled changed";
        builder.insertProperty(imAccount, "nco:imEnabled", LiteralValue(account->isEnabled()),
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
                "?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ].\n"
                "FILTER(str(?_imAddress) IN %1).");
        CDTpQueryBuilder subBuilder("Contacts Added/Removed - update timestamp");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(contactsAdded).sparql()));
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
                "?_imAddress nco:imCapability ?_v.\n"
                "FILTER(str(?_imAddress) IN %1).");
        CDTpQueryBuilder subBuilder("UpdateContacts - remove caps");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(capsContacts).sparql()));
        subBuilder.deleteProperty(imAddressVar, "nco:imCapability", Variable(QLatin1String("v")));
        builder.prependRawQuery(subBuilder);
    }

    // delete ContactInfo for contacts who's info changed
    if (!infoContacts.isEmpty()) {
        static const QString tmpl = QString::fromLatin1(
                "?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ].\n"
                "FILTER(str(?_imAddress) IN %1).");
        CDTpQueryBuilder subBuilder("UpdateContacts - remove info");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(infoContacts).sparql()));
        addRemoveContactInfoToBuilder(subBuilder, imAddressVar, imContactVar);
        builder.appendRawQuery(subBuilder);
    }

    // Create ContactInfo for each contact who's info changed
    Q_FOREACH (const CDTpContactPtr contactWrapper, infoContacts) {
        builder.appendRawQuery(createContactInfoBuilder(contactWrapper));
    }

    // Update nie:contentLastModified on all nco:PersonContact bound to contacts
    static const QString tmpl = QString::fromLatin1(
            "?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ].\n"
            "FILTER(str(?_imAddress) IN %1).");
    CDTpQueryBuilder subBuilder("UpdateContacts - update timestamp");
    subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(allContacts).sparql()));
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
            "?_imAccount nco:hasIMContact ?_imAddress.\n"
            "?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ].\n"
            "FILTER(str(?_imAddress) NOT IN %1 && str(?_imAccount) IN %2).");
    CDTpQueryBuilder builder("SyncNoRosterAccountsContacts");
    builder.appendRawSelection(tmpl.arg(literalIMAddressList(accounts).sparql()).arg(literalIMAccountList(accounts).sparql()));
    addPresenceToBuilder(builder, imAddressVar, unknownPresence);
    builder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());

    // Remove capabilities from all imAddresses
    static const QString tmpl2 = QString::fromLatin1(
            "?_imAccount nco:hasIMContact ?_imAddress.\n"
            "FILTER(str(?_imAccount) IN %1).");
    CDTpQueryBuilder subBuilder("SyncNoRosterAccountsContacts - remove caps");
    subBuilder.appendRawSelection(tmpl2.arg(literalIMAccountList(accounts).sparql()));
    addRemoveCapabilitiesToBuilder(subBuilder, imAddressVar);
    builder.appendRawQuery(subBuilder);

    // Add capabilities on all contacts for each account
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        static const QString tmpl = QString::fromLatin1(
                "%1 nco:hasIMContact ?_imAddress.");
        CDTpQueryBuilder subBuilder("SyncNoRosterAccountsContacts - add caps");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAccount(accountWrapper).sparql()));
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
            "?_imAccount nco:hasIMContact ?_imAddress.\n"
            "FILTER(str(?_imAddress) NOT IN %1 && str(?_imAccount) IN %2).");
    builder.appendRawSelection(tmpl.arg(literalIMAddressList(accounts).sparql()).arg(literalIMAccountList(accounts).sparql()));
    builder.deleteProperty(imAccountVar, "nco:hasIMContact", imAddressVar);
    addRemoveCapabilitiesToBuilder(builder, imAddressVar);

    /* Delete ContactInfo for those who know it */
    QList<CDTpContactPtr> infoContacts;
    Q_FOREACH (const CDTpContactPtr contactWrapper, allContacts) {
        if (contactWrapper->isInformationKnown()) {
            infoContacts << contactWrapper;
        }
    }
    if (!infoContacts.isEmpty()) {
        CDTpQueryBuilder subBuilder("SyncRosterAccounts  - remove known ContactInfo");
        static const QString tmpl = QString::fromLatin1(
                "?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ].\n"
                "FILTER(str(?_imAddress) IN %1)");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAddressList(infoContacts).sparql()));
        addRemoveContactInfoToBuilder(subBuilder, imAddressVar, imContactVar);
        builder.appendRawQuery(subBuilder);
    }

    /* Now create all contacts */
    if (!allContacts.isEmpty()) {
        builder.appendRawQuery(createContactsBuilder(allContacts));

        /* Update timestamp on all nco:PersonContact bound to this account */
        static const QString tmpl = QString::fromLatin1(
                "?_imAccount nco:hasIMContact ?_imAddress.\n"
                "?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ].\n"
                "FILTER(str(?_imAccount) IN %1).");
        CDTpQueryBuilder subBuilder("SyncRosterAccounts - update timestamp");
        subBuilder.appendRawSelection(tmpl.arg(literalIMAccountList(accounts).sparql()));
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

        const Value imAddress = literalIMAddress(contactWrapper);
        builder.createResource(imAddress, "nco:IMAddress", privateGraph);
        builder.insertProperty(imAddress, "nco:imID", LiteralValue(contactWrapper->contact()->id()),
                privateGraph);
        builder.insertProperty(imAddress, "nco:imProtocol", LiteralValue(accountWrapper->account()->protocolName()),
                privateGraph);

        const Value imAccount = literalIMAccount(accountWrapper);
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
        "                      nco:hasIMAddress ?_imAddress;\n"
        "                      rdfs:label \"Other\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    GRAPH %2 { ?_imAddress nco:imID ?imID }.\n"
        "    FILTER (NOT EXISTS { ?_imContact nco:hasAffiliation [ nco:hasIMAddress ?_imAddress ] })\n"
        "}\n");
    builder.appendRawQuery(tmpl.arg(CDTpQueryBuilder::defaultGraph.sparql()).arg(privateGraph.sparql())
            .arg(literalTimeStamp().sparql()).arg(defaultGenerator));

    // Add ContactInfo seperately because we need to bind to the PersonContact
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        builder.appendRawQuery(createContactInfoBuilder(contactWrapper));
    }

    return builder;
}

void CDTpStorage::addContactChangesToBuilder(CDTpQueryBuilder &builder,
        const Value &imAddress,
        CDTpContact::Changes changes,
        Tp::ContactPtr contact) const
{
    debug() << "Update contact" << imAddress.sparql();

    // Apply changes
    if (changes & CDTpContact::Alias) {
        debug() << "  alias changed";
        builder.insertProperty(imAddress, "nco:imNickname", LiteralValue(contact->alias().trimmed()),
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
        const Value &imAddress,
        const Tp::Presence &presence) const
{
    builder.insertProperty(imAddress, "nco:imPresence", presenceType(presence.type()),
            privateGraph);
    builder.insertProperty(imAddress, "nco:presenceLastModified", literalTimeStamp(),
            privateGraph);
    builder.insertProperty(imAddress, "nco:imStatusMessage", LiteralValue(presence.statusMessage()),
            privateGraph);
}

void CDTpStorage::addRemoveCapabilitiesToBuilder(CDTpQueryBuilder &builder,
        const Value &imAddress) const
{
    builder.deleteProperty(imAddress, "nco:imCapability");
}

void CDTpStorage::addCapabilitiesToBuilder(CDTpQueryBuilder &builder,
        const Value &imAddress,
        Tp::CapabilitiesBase capabilities) const
{
    if (capabilities.textChats()) {
        static const Value value = ResourceValue(QString::fromLatin1("nco:im-capability-text-chat"), ResourceValue::PrefixedName);
        builder.insertProperty(imAddress, "nco:imCapability", value, privateGraph);
    }
    if (capabilities.streamedMediaAudioCalls()) {
        static const Value value = ResourceValue(QString::fromLatin1("nco:im-capability-audio-calls"), ResourceValue::PrefixedName);
        builder.insertProperty(imAddress, "nco:imCapability", value, privateGraph);
    }
    if (capabilities.streamedMediaVideoCalls()) {
        static const Value value = ResourceValue(QString::fromLatin1("nco:im-capability-video-calls"), ResourceValue::PrefixedName);
        builder.insertProperty(imAddress, "nco:imCapability", value, privateGraph);
    }
}

void CDTpStorage::addAvatarToBuilder(CDTpQueryBuilder &builder,
        const Value &imAddress,
        const QString &fileName) const
{
    if (!fileName.isEmpty()) {
        const QUrl url = QUrl::fromLocalFile(fileName);
        const Value dataObject = ResourceValue(url);
        builder.createResource(dataObject, "nfo:FileDataObject", privateGraph);
        builder.insertProperty(dataObject, "nie:url", LiteralValue(url), privateGraph);
        builder.insertProperty(imAddress, "nco:imAvatar", dataObject, privateGraph);
    }
}

CDTpQueryBuilder CDTpStorage::createContactInfoBuilder(CDTpContactPtr contactWrapper) const
{
    if (!contactWrapper->isInformationKnown()) {
        debug() << "contact information is unknown";
        return CDTpQueryBuilder();
    }

    Tp::ContactInfoFieldList listContactInfo = contactWrapper->contact()->infoFields().allFields();
    if (listContactInfo.count() == 0) {
        debug() << "contact information is empty";
        return CDTpQueryBuilder();
    }

    /* Use the imAddress as graph for ContactInfo fields, so we can easilly
     * know from which contact it comes from */
    const Value graph = literalIMAddress(contactWrapper);

    /* Create a builder with ?imContact bound to this contact */
    static const QString tmpl = QString::fromLatin1("?_imContact nco:hasAffiliation [ nco:hasIMAddress %1 ].");
    CDTpQueryBuilder builder("CreateContactInfo");
    builder.appendRawSelection(tmpl.arg(literalIMAddress(contactWrapper).sparql()));

    QHash<QString, Value> affiliations;
    Q_FOREACH (const Tp::ContactInfoField &field, listContactInfo) {
        if (field.fieldValue.count() == 0) {
            continue;
        }

        /* Extract field types */
        QStringList subTypes;
        QString affiliationLabel = QLatin1String("Other");
        Q_FOREACH (const QString &param, field.parameters) {
            if (!param.startsWith(QLatin1String("type="))) {
                continue;
            }
            const QString type = param.mid(5);
            if (type == QLatin1String("home")) {
                affiliationLabel = QLatin1String("Home");
            } else if (type == QLatin1String("work")) {
                affiliationLabel = QLatin1String("Work");
            } else {
                subTypes << param.mid(5);
            }
        }

        /* FIXME:
         *  - Do we care about "fn" and "nickname" ?
         *  - How do we write affiliation for "org" ?
         */
        if (!field.fieldName.compare(QLatin1String("tel"))) {
            static QHash<QString, QString> knownTypes;
            if (knownTypes.isEmpty()) {
                knownTypes.insert(QLatin1String("bbsl"), QLatin1String("nco:BbsNumber"));
                knownTypes.insert(QLatin1String("car"), QLatin1String("nco:CarPhoneNumber"));
                knownTypes.insert(QLatin1String("cell"), QLatin1String("nco:CellPhoneNumber"));
                knownTypes.insert(QLatin1String("fax"), QLatin1String("nco:FaxNumber"));
                knownTypes.insert(QLatin1String("isdn"), QLatin1String("nco:IsdnNumber"));
                knownTypes.insert(QLatin1String("modem"), QLatin1String("nco:ModemNumber"));
                knownTypes.insert(QLatin1String("pager"), QLatin1String("nco:PagerNumber"));
                knownTypes.insert(QLatin1String("pcs"), QLatin1String("nco:PcsNumber"));
                knownTypes.insert(QLatin1String("video"), QLatin1String("nco:VideoTelephoneNumber"));
                knownTypes.insert(QLatin1String("voice"), QLatin1String("nco:VoicePhoneNumber"));
            }

            QStringList resourceTypes = QStringList() << QLatin1String("nco:PhoneNumber");
            Q_FOREACH (const QString &type, subTypes) {
                if (knownTypes.contains(type)) {
                    resourceTypes << knownTypes[type];
                }
            }

            const Value affiliation = ensureContactAffiliationToBuilder(builder, affiliations, affiliationLabel, imContactVar, graph);
            const Value phoneNumber = qctMakePhoneNumberResource(field.fieldValue[0], subTypes);
            builder.createResource(phoneNumber, resourceTypes);
            builder.insertProperty(phoneNumber, "nco:phoneNumber", literalContactInfo(field, 0));
            builder.insertProperty(phoneNumber, "maemo:localPhoneNumber",
                    LiteralValue(qctMakeLocalPhoneNumber(field.fieldValue[0])));
            builder.insertProperty(affiliation, "nco:hasPhoneNumber", phoneNumber);
        }

        else if (!field.fieldName.compare(QLatin1String("email"))) {
            // FIXME: Should normalize email address?
            static const QString tmpl = QString::fromLatin1("mailto:%1");
            const Value emailAddress = ResourceValue(tmpl.arg(field.fieldValue[0]));
            const Value affiliation = ensureContactAffiliationToBuilder(builder, affiliations, affiliationLabel, imContactVar, graph);
            builder.createResource(emailAddress, "nco:EmailAddress");
            builder.insertProperty(emailAddress, "nco:emailAddress", literalContactInfo(field, 0));
            builder.insertProperty(affiliation, "nco:hasEmailAddress", emailAddress);
        }

        else if (!field.fieldName.compare(QLatin1String("adr"))) {
            static QHash<QString, QString> knownTypes;
            if (knownTypes.isEmpty()) {
                knownTypes.insert(QLatin1String("dom"), QLatin1String("nco:DomesticDeliveryAddress"));
                knownTypes.insert(QLatin1String("intl"), QLatin1String("nco:InternationalDeliveryAddress"));
                knownTypes.insert(QLatin1String("parcel"), QLatin1String("nco:ParcelDeliveryAddress"));
            }

            QStringList resourceTypes = QStringList() << QLatin1String("nco:PostalAddress");
            Q_FOREACH (const QString &type, subTypes) {
                if (knownTypes.contains(type)) {
                    resourceTypes << knownTypes[type];
                }
            }

            const Value affiliation = ensureContactAffiliationToBuilder(builder, affiliations, affiliationLabel, imContactVar, graph);
            const Value postalAddress = BlankValue(builder.uniquify("address"));
            builder.createResource(postalAddress, resourceTypes);
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
            const Value affiliation = ensureContactAffiliationToBuilder(builder, affiliations, affiliationLabel, imContactVar, graph);
            builder.insertProperty(affiliation, "nco:url", literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("title"))) {
            const Value affiliation = ensureContactAffiliationToBuilder(builder, affiliations, affiliationLabel, imContactVar, graph);
            builder.insertProperty(affiliation, "nco:title", literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("role"))) {
            const Value affiliation = ensureContactAffiliationToBuilder(builder, affiliations, affiliationLabel, imContactVar, graph);
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
                builder.insertProperty(imContactVar, "nco:birthDate", LiteralValue(QDateTime(date)), graph);
            } else {
                debug() << "Unsupported bday format:" << field.fieldValue[0];
            }
        }

        else if (!field.fieldName.compare(QLatin1String("x-gender"))) {
            if (field.fieldValue[0] == QLatin1String("male")) {
                static const Value maleIri = ResourceValue(QLatin1String("nco:gender-male"), ResourceValue::PrefixedName);
                builder.insertProperty(imContactVar, "nco:gender", maleIri, graph);
            } else if (field.fieldValue[0] == QLatin1String("female")) {
                static const Value femaleIri = ResourceValue(QLatin1String("nco:gender-female"), ResourceValue::PrefixedName);
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

Value CDTpStorage::ensureContactAffiliationToBuilder(CDTpQueryBuilder &builder,
        QHash<QString, Value> &affiliations,
        QString affiliationLabel,
        const Value &imContact,
        const Value &graph) const
{
    if (!affiliations.contains(affiliationLabel)) {
        const Value affiliation = BlankValue(builder.uniquify("affiliation"));
        builder.createResource(affiliation, "nco:Affiliation");
        builder.insertProperty(affiliation, "rdfs:label", LiteralValue(affiliationLabel));
        builder.insertProperty(imContact, "nco:hasAffiliation", affiliation, graph);
        affiliations.insert(affiliationLabel, affiliation);
    }

    return affiliations[affiliationLabel];
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
    const Value imAccount = literalIMAccount(accountPath);
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
    static const Value affiliationVar = Variable(QString::fromLatin1("affiliation"));
    static const QString tmpl1 = QString::fromLatin1(
            "GRAPH %1 { ?_imAddress nco:imID ?imID }.\n"
            "?_imContact nco:hasAffiliation ?_affiliation.\n"
            "?_affiliation nco:hasIMAddress ?_imAddress.\n"
            "FILTER(NOT EXISTS { ?_imAccount nco:hasIMContact ?_imAddress }).")
            .arg(privateGraph.sparql());
    CDTpQueryBuilder builder("PurgeContacts - step 1");
    builder.appendRawSelection(tmpl1);
    builder.deleteProperty(imContactVar, "nie:contentLastModified");
    builder.deleteProperty(affiliationVar, "nco:hasIMAddress", imAddressVar);
    addRemoveContactInfoToBuilder(builder, imAddressVar, imContactVar);

    /* Step 1.1 - Remove the nco:IMAddress resource.
     * This must be done in a separate query because of NB#242979 */
    CDTpQueryBuilder subBuilder("PurgeContacts - step 1.1");
    static const QString tmpl11 = QString::fromLatin1(
            "GRAPH %1 { ?_imAddress nco:imID ?imID }.\n"
            "FILTER(NOT EXISTS { ?_imAccount nco:hasIMContact ?_imAddress }).")
            .arg(privateGraph.sparql());
    subBuilder.appendRawSelection(tmpl11);
    subBuilder.deleteResource(imAddressVar);
    builder.appendRawQuery(subBuilder);

    /* Step 2 - Purge nco:PersonContact with generator "telepathy" but with no
     * nco:IMAddress bound anymore */
    subBuilder = CDTpQueryBuilder("PurgeContacts - step 2");
    static const QString tmpl2 = QString::fromLatin1(
            "?_imContact nie:generator %1.\n"
            "FILTER(NOT EXISTS { ?_imContact nco:hasAffiliation [nco:hasIMAddress ?v] })")
            .arg(defaultGenerator);
    subBuilder.appendRawSelection(tmpl2);
    subBuilder.deleteResource(imContactVar);
    builder.appendRawQuery(subBuilder);

    /* Step 3 - Add back nie:contentLastModified for nco:PersonContact missing one */
    subBuilder = CDTpQueryBuilder("PurgeContacts - step 3");
    static const QString tmpl3 = QString::fromLatin1(
            "?_imContact a nco:PersonContact.\n"
            "FILTER(NOT EXISTS { ?_imContact nie:contentLastModified ?v }).");
    subBuilder.appendRawSelection(tmpl3);
    subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
    builder.appendRawQuery(subBuilder);

    return builder;
}

void CDTpStorage::addRemoveContactInfoToBuilder(CDTpQueryBuilder &builder,
        const Value &imAddress,
        const Value &imContact) const
{
    /* imAddress is used as graph for properties on the imContact */
    const Value graph = imAddress;

    /* Remove all triples on imContact and in graph. All sub-resources will be
     * GCed by qct sometimes */
    builder.deletePropertyWithGraph(imContact, "nco:birthDate", graph);
    builder.deletePropertyWithGraph(imContact, "nco:gender", graph);
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

Value CDTpStorage::presenceType(Tp::ConnectionPresenceType presenceType) const
{
    static const Value statusUnknown = ResourceValue(QString::fromLatin1("nco:presence-status-unknown"), ResourceValue::PrefixedName);
    static const Value statusOffline = ResourceValue(QString::fromLatin1("nco:presence-status-offline"), ResourceValue::PrefixedName);
    static const Value statusAvailable = ResourceValue(QString::fromLatin1("nco:presence-status-available"), ResourceValue::PrefixedName);
    static const Value statusAway = ResourceValue(QString::fromLatin1("nco:presence-status-away"), ResourceValue::PrefixedName);
    static const Value statusExtendedAway = ResourceValue(QString::fromLatin1("nco:presence-status-extended-away"), ResourceValue::PrefixedName);
    static const Value statusHidden = ResourceValue(QString::fromLatin1("nco:presence-status-hidden"), ResourceValue::PrefixedName);
    static const Value statusBusy = ResourceValue(QString::fromLatin1("nco:presence-status-busy"), ResourceValue::PrefixedName);
    static const Value statusError = ResourceValue(QString::fromLatin1("nco:presence-status-error"), ResourceValue::PrefixedName);

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

Value CDTpStorage::presenceState(Tp::Contact::PresenceState presenceState) const
{
    static const Value statusNo = ResourceValue(QString::fromLatin1("nco:predefined-auth-status-no"), ResourceValue::PrefixedName);
    static const Value statusRequested = ResourceValue(QString::fromLatin1("nco:predefined-auth-status-requested"), ResourceValue::PrefixedName);
    static const Value statusYes = ResourceValue(QString::fromLatin1("nco:predefined-auth-status-yes"), ResourceValue::PrefixedName);

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

Value CDTpStorage::literalTimeStamp() const
{
    return LiteralValue(QDateTime::currentDateTime());
}

static QString imAddress(const QString &accountPath, const QString &contactId)
{
    static const QString tmpl = QString::fromLatin1("telepathy:%1!%2");
    return tmpl.arg(accountPath, contactId);
}

static QString imAccount(const QString &accountPath)
{
    static const QString tmpl = QString::fromLatin1("telepathy:%1");
    return tmpl.arg(accountPath);
}

Value CDTpStorage::literalIMAddress(const QString &accountPath, const QString &contactId, bool resource) const
{
    if (resource) {
        return ResourceValue(imAddress(accountPath, contactId));
    }
    return LiteralValue(imAddress(accountPath, contactId));
}

Value CDTpStorage::literalIMAddress(const CDTpContactPtr &contactWrapper, bool resource) const
{
    const QString accountPath = contactWrapper->accountWrapper()->account()->objectPath();
    const QString contactId = contactWrapper->contact()->id();
    return literalIMAddress(accountPath, contactId, resource);
}

Value CDTpStorage::literalIMAddress(const CDTpAccountPtr &accountWrapper, bool resource) const
{
    static const QString contactId = QString::fromLatin1("self");
    return literalIMAddress(accountWrapper->account()->objectPath(), contactId, resource);
}

Value CDTpStorage::literalIMAddressList(const QList<CDTpContactPtr> &contacts) const
{
    ValueList list;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        list.addValue(literalIMAddress(contactWrapper, false));
    }
    return list;
}

Value CDTpStorage::literalIMAddressList(const QList<CDTpAccountPtr> &accounts) const
{
    ValueList list;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        list.addValue(literalIMAddress(accountWrapper, false));
    }
    return list;
}

Value CDTpStorage::literalIMAccount(const QString &accountPath, bool resource) const
{
    if (resource) {
        return ResourceValue(imAccount(accountPath));
    }
    return LiteralValue(imAccount(accountPath));
}

Value CDTpStorage::literalIMAccount(const CDTpAccountPtr &accountWrapper, bool resource) const
{
    return literalIMAccount(accountWrapper->account()->objectPath(), resource);
}

Value CDTpStorage::literalIMAccountList(const QList<CDTpAccountPtr> &accounts) const
{
    ValueList list;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        list.addValue(literalIMAccount(accountWrapper, false));
    }
    return list;
}

Value CDTpStorage::literalContactInfo(const Tp::ContactInfoField &field, int i) const
{
    if (i >= field.fieldValue.count()) {
        return LiteralValue(QLatin1String(""));
    }

    return LiteralValue(field.fieldValue[i]);
}

