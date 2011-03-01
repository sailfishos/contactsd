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
    CDTpQueryBuilder builder("SyncAccounts");

    /* Ensure the default-contact-me exists. NB#215973 */
    builder.createResource(defaultContactMe, "nco:PersonContact");

    QStringList imAccounts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        imAccounts << literalIMAccount(accountWrapper);
    }

    /* Selection for all IMAccounts that does not exist anymore, and their IMAddresses */
    static const QString tmpl = QString::fromLatin1("?imAccount nco:hasIMContact ?imAddress.");
    QString selection = tmpl;
    if (!accounts.isEmpty()) {
        static const QString tmpl = QString::fromLatin1("\nFILTER (?imAccount NOT IN (%1)).");
        selection += tmpl.arg(imAccounts.join(filterInSeparator));
    }

    /* Purge contacts for accounts that does not exist anymore */
    CDTpQueryBuilder subBuilder("SyncAccounts - purge contacts");
    subBuilder.appendRawSelection(selection);
    addRemoveContactToBuilder(subBuilder, imAddressVar);
    builder.appendRawQuery(subBuilder);

    /* Purge accounts/imAddresses that does not exist anymore */
    /* FIXME: We leak avatar object */
    subBuilder = CDTpQueryBuilder("SyncAccounts - purge accounts/imAddresses");
    subBuilder.appendRawSelection(selection);
    subBuilder.deleteResource(imAccountVar);
    subBuilder.deleteResource(imAddressVar);
    builder.appendRawQuery(subBuilder);

    /* Sync accounts and their contacts */
    if (!accounts.isEmpty()) {
        subBuilder = CDTpQueryBuilder("SyncAccounts - sync accounts");
        addSyncAccountsToBuilder(subBuilder, accounts);
        builder.appendRawQuery(subBuilder);
    }

    /* Notify import progress for accounts that have contacts */
    QList<CDTpAccountPtr> rosterAccounts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        if (!accountWrapper->contacts().isEmpty()) {
            rosterAccounts << accountWrapper;
            Q_EMIT syncStarted(accountWrapper);
        }
    }
    CDTpAccountsSparqlQuery *query = new CDTpAccountsSparqlQuery(rosterAccounts, builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSyncOperationEnded(CDTpSparqlQuery *)));
}

void CDTpStorage::syncAccount(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder("SyncAccount");

    /* Create account */
    QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
    builder.updateProperty(defaultContactMe, "nie:contentLastModified", literalTimeStamp());
    addCreateAccountsToBuilder(builder, accounts);

    /* if account has no contacts, we are done */
    if (accountWrapper->contacts().isEmpty()) {
        new CDTpSparqlQuery(builder, this);
        return;
    }

    /* Create account's contacts */
    addCreateContactsToBuilder(builder, accountWrapper->contacts());

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

    builder.updateProperty(defaultContactMe, "nie:contentLastModified", literalTimeStamp());

    addRemoveAccountsChangesToBuilder(builder,
        literalIMAddress(accountWrapper), literalIMAccount(accountWrapper),
        changes);
    addAccountChangesToBuilder(builder, accountWrapper, changes);

    new CDTpSparqlQuery(builder, this);
}

void CDTpStorage::removeAccount(CDTpAccountPtr accountWrapper)
{
    const QString imAccount = literalIMAccount(accountWrapper);
    debug() << "Remove account" << imAccount;

    /* Bind imAddress to all contacts of this imAccount */
    static const QString tmpl = QString::fromLatin1("%1 nco:hasIMContact ?imAddress.");
    const QString selection = tmpl.arg(imAccount);

    /* Remove account's contacts */
    CDTpQueryBuilder builder("RemoveAccount - contacts");
    builder.appendRawSelection(selection);
    addRemoveContactToBuilder(builder, imAddressVar);

    /* Remove imAddress */
    /* FIXME: We leak avatar object */
    CDTpQueryBuilder subBuilder("RemoveAccount - imAddress");
    subBuilder.appendRawSelection(selection);
    subBuilder.deleteResource(imAddressVar);
    builder.appendRawQuery(subBuilder);

    /* Remove account */
    subBuilder = CDTpQueryBuilder("RemoveAccount - account");
    subBuilder.deleteResource(imAccount);
    builder.appendRawQuery(subBuilder);

    new CDTpSparqlQuery(builder, this);
}

void CDTpStorage::addSyncAccountsToBuilder(CDTpQueryBuilder &builder,
        const QList<CDTpAccountPtr> &accounts) const
{
    QStringList imAccounts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        imAccounts << literalIMAccount(accountWrapper);
    }

    // Drop all mutable properties in all accounts at once, then (re-)create them
    static const QString tmpl = QString::fromLatin1(
            "?imAccount nco:imAccountAddress ?imAddress.\n"
            "FILTER(?imAccount IN (%1)).");
    builder.appendRawSelection(tmpl.arg(imAccounts.join(filterInSeparator)));

    addRemoveAccountsChangesToBuilder(builder, imAddressVar, imAccountVar, CDTpAccount::All);

    CDTpQueryBuilder subBuilder("CreateAccounts");

    subBuilder.updateProperty(defaultContactMe, "nie:contentLastModified", literalTimeStamp());
    addCreateAccountsToBuilder(subBuilder, accounts);
    builder.appendRawQuery(subBuilder);

    // Sync all contacts of all accounts. Special case for no-roster/offline
    // or disabled accounts.
    QList<CDTpAccountPtr> rosterAccounts;
    QList<CDTpAccountPtr> disabledAccounts;
    QList<CDTpAccountPtr> noRosterAccounts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        if (accountWrapper->hasRoster()) {
            rosterAccounts << accountWrapper;
        } else if (!accountWrapper->account()->isEnabled()) {
            disabledAccounts << accountWrapper;
        } else {
            noRosterAccounts << accountWrapper;
        }
    }

    if (!rosterAccounts.isEmpty()) {
        subBuilder = CDTpQueryBuilder("SyncRosterAccounts");
        addSyncRosterAccountsContactsToBuilder(subBuilder, rosterAccounts);
        builder.appendRawQuery(subBuilder);
    }

    if (!disabledAccounts.isEmpty()) {
        subBuilder = CDTpQueryBuilder("SyncDisabledAccounts");
        addSyncDisabledAccountsContactsToBuilder(subBuilder, disabledAccounts);
        builder.appendRawQuery(subBuilder);
    }

    if (!noRosterAccounts.isEmpty()) {
        subBuilder = CDTpQueryBuilder("SyncNoRosterAccounts");
        addSyncNoRosterAccountsContactsToBuilder(subBuilder, noRosterAccounts);
        builder.appendRawQuery(subBuilder);
    }
}

void CDTpStorage::addCreateAccountsToBuilder(CDTpQueryBuilder &builder,
        const QList<CDTpAccountPtr> &accounts) const
{
    QStringList imAddresses;
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
        builder.createResource(imAddress, "nco:IMAddress");
        builder.insertProperty(imAddress, "nco:imID", literal(account->normalizedName()));
        builder.insertProperty(imAddress, "nco:imProtocol", literal(account->protocolName()));

        // Add all mutable properties
        addAccountChangesToBuilder(builder, accountWrapper, CDTpAccount::All);

        imAddresses << imAddress;
    }

    // Ensure the IMAddresses are bound to the default-contact-me via an affiliation
    static const QString tmpl = QString::fromLatin1(
        "INSERT {\n"
        "    GRAPH %1 {\n"
        "        nco:default-contact-me nco:hasAffiliation _:affiliation.\n"
        "        _:affiliation a nco:Affiliation;\n"
        "                      nco:hasIMAddress ?imAddress;\n"
        "                      rdfs:label \"Other\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    ?imAddress a nco:IMAddress.\n"
        "    FILTER (?imAddress IN (%2) &&\n"
        "            NOT EXISTS { nco:default-contact-me nco:hasAffiliation [ nco:hasIMAddress ?imAddress ] })\n"
        "}\n");
    builder.appendRawQuery(tmpl.arg(CDTpQueryBuilder::defaultGraph).arg(imAddresses.join(filterInSeparator)));
}

void CDTpStorage::addRemoveAccountsChangesToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &imAccount,
        CDTpAccount::Changes changes) const
{
    if (changes & CDTpAccount::Presence) {
        addRemovePresenceToBuilder(builder, imAddress);
    }
    if (changes & CDTpAccount::Avatar) {
        addRemoveAvatarToBuilder(builder, imAddress);
    }
    if (changes & CDTpAccount::Nickname) {
        builder.deleteProperty(imAddress, "nco:imNickname");
    }
    if (changes & CDTpAccount::DisplayName) {
        builder.deleteProperty(imAccount, "nco:imDisplayName");
    }
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
        builder.insertProperty(imAddress, "nco:imNickname", literal(account->nickname()));
    }
    if (changes & CDTpAccount::DisplayName) {
        debug() << "  display name changed";
        builder.insertProperty(imAccount, "nco:imDisplayName", literal(account->displayName()),
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

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder("SyncAccountContacts");

    QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
    if (accountWrapper->hasRoster()) {
        addSyncRosterAccountsContactsToBuilder(builder, accounts);
    } else if (!accountWrapper->account()->isEnabled()) {
        addSyncDisabledAccountsContactsToBuilder(builder, accounts);
    } else {
        addSyncNoRosterAccountsContactsToBuilder(builder, accounts);
    }

    /* if account has no contacts, we are done */
    if (accountWrapper->contacts().isEmpty()) {
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
        // delete the timestamp from PersonContact linked to imAddresses
        // (unlikely to exist). addCreateContactsToBuilder() will insert a new
        // value.
        QStringList imAddresses;
        Q_FOREACH (const CDTpContactPtr &contactWrapper, contactsAdded) {
            imAddresses << literalIMAddress(contactWrapper);
        }
        static const QString tmpl = QString::fromLatin1(
                "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
                "FILTER(?imAddress IN (%1)).");
        builder.appendRawSelection(tmpl.arg(imAddresses.join(filterInSeparator)));
        builder.deleteProperty(imContactVar, "nie:contentLastModified");

        // Create contacts
        CDTpQueryBuilder subBuilder("Contacts Added/Removed - Create contacts");
        addCreateContactsToBuilder(subBuilder, contactsAdded);
        builder.appendRawQuery(subBuilder);
    }

    if (!contactsRemoved.isEmpty()) {
        addRemoveContactsToBuilder(builder, accountWrapper, contactsRemoved);
    }

    new CDTpSparqlQuery(builder, this);
}

/* Use this only in offline mode - use syncAccountContacts in online mode */
void CDTpStorage::removeAccountContacts(const QString &accountPath, const QStringList &contactIds)
{
    CDTpQueryBuilder builder;
    addRemoveContactsToBuilder(builder, accountPath, contactIds);
    new CDTpSparqlQuery(builder, this);
}

void  CDTpStorage::contactsToAvoid(const QString &accountPath, const QStringList &contactIds)
{
   mContactsToAvoid[accountPath] = contactIds;
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

    bool found = false;
    CDTpQueryBuilder builder("UpdateContacts");
    QHash<CDTpContactPtr, CDTpContact::Changes>::const_iterator i;
    for (i = mUpdateQueue.constBegin(); i != mUpdateQueue.constEnd(); i++) {
        CDTpContactPtr contactWrapper = i.key();
        CDTpContact::Changes changes = i.value();
        if (!contactWrapper->isVisible()) {
            continue;
        }

        CDTpQueryBuilder subBuilder("UpdateContact");

        // bind imContact to imAddresses
        static const QString tmpl = QString::fromLatin1(
                "?imContact nco:hasAffiliation [ nco:hasIMAddress %1 ].");

        const QString imAddress = literalIMAddress(contactWrapper);
        subBuilder.appendRawSelection(tmpl.arg(imAddress));
        addRemoveContactsChangesToBuilder(subBuilder, imAddress, imContactVar, changes);
        addContactChangesToBuilder(subBuilder, imAddress, imContactVar, changes, contactWrapper->contact());
        builder.appendRawQuery(subBuilder);

        found = true;
    }
    mUpdateQueue.clear();

    if (!found) {
        debug() << "  None needs update";
        return;
    }

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

void CDTpStorage::addSyncNoRosterAccountsContactsToBuilder(CDTpQueryBuilder &builder,
        const QList<CDTpAccountPtr> accounts) const
{
    QStringList imAccounts;
    QStringList imAddresses;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        const QString imAddress = literalIMAddress(accountWrapper);
        const QString imAccount = literalIMAccount(accountWrapper);

        debug() << "Sync no roster account" << imAddress;

        imAddresses << imAddress;
        imAccounts << imAccount;
    }

    /* Bind ?imAccount to all no roster accounts.
     * Bind ?imAddress to all accounts' im contacts, except for the self contact
     * because we know it has presence OFFLINE, and that's done when updating
     * account.
     * Bind ?imContact to all contacts' local contact.
     */
    static const QString tmpl = QString::fromLatin1(
            "?imAccount nco:hasIMContact ?imAddress.\n"
            "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
            "FILTER(?imAddress NOT IN (%1) && ?imAccount IN (%2)).");
    builder.appendRawSelection(tmpl.arg(imAddresses.join(filterInSeparator)).arg(imAccounts.join(filterInSeparator)));

    // FIXME: we could use addPresenceToBuilder() somehow
    builder.updateProperty(imAddressVar, "nco:imPresence", presenceType(Tp::ConnectionPresenceTypeUnknown));
    builder.updateProperty(imAddressVar, "nco:presenceLastModified", literalTimeStamp());
    builder.updateProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());

    // Update capabilities of all contacts, since we don't know them anymore,
    // reset them to the account's caps.
    addRemoveCapabilitiesToBuilder(builder, imAddressVar);

    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        addCapabilitiesToBuilder(builder, literalIMAddress(accountWrapper),
                accountWrapper->account()->capabilities());
    }
}

void CDTpStorage::addSyncDisabledAccountsContactsToBuilder(CDTpQueryBuilder &builder,
        const QList<CDTpAccountPtr> accounts) const
{
    // FIXME: disabled for now
    addSyncNoRosterAccountsContactsToBuilder(builder, accounts);
}

void CDTpStorage::addSyncRosterAccountsContactsToBuilder(CDTpQueryBuilder &builder,
        const QList<CDTpAccountPtr> &accounts) const
{
    /* Remove all mutable info from all contacts of those accounts */
    addRemoveContactsChangesToBuilder(builder, accounts);

    /* Delete the hasIMContact property on IMAccounts (except for self contact)
     * then sync all contacts (that will add back hasIMContact for them). After
     * that we can purge all imAddresses not bound to an IMAccount anymore. */
    QStringList imAccounts;
    QStringList imAddresses;
    QList<CDTpContactPtr> allContacts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        const QString imAddress = literalIMAddress(accountWrapper);
        const QString imAccount = literalIMAccount(accountWrapper);
        const QString accountPath = accountWrapper->account()->objectPath();

        debug() << "Sync roster account" << imAddress;

        imAccounts << imAccount;
        imAddresses << imAddress;
        Q_FOREACH(CDTpContactPtr contactPtr, accountWrapper->contacts()) {
            QStringList offlineRemoved = mContactsToAvoid[accountPath];
            if (!offlineRemoved.contains(contactPtr->contact()->id())) {
                allContacts << contactPtr;
            }
        }
    }
    CDTpQueryBuilder subBuilder = CDTpQueryBuilder("SyncRosterAccounts - drop hasIMContact");
    static const QString tmpl = QString::fromLatin1(
            "?imAccount nco:hasIMContact ?imAddress.\n"
            "FILTER(?imAddress NOT IN (%1) && ?imAccount IN (%2)).");
    subBuilder.appendRawSelection(tmpl.arg(imAddresses.join(filterInSeparator)).arg(imAccounts.join(filterInSeparator)));
    subBuilder.deleteProperty(imAccountVar, "nco:hasIMContact", imAddressVar);
    builder.appendRawQuery(subBuilder);

    /* Now create all contacts */
    if (!allContacts.isEmpty()) {
        subBuilder = CDTpQueryBuilder("CreateContacts");
        addCreateContactsToBuilder(subBuilder, allContacts);
        builder.appendRawQuery(subBuilder);
    }

    /* Bind imAddress to all contacts that does not exist anymore, to purge them */
    static const QString selection = QString::fromLatin1(
            "?imAddress a nco:IMAddress.\n"
            "OPTIONAL { ?imAccount nco:hasIMContact ?imAddress }.\n"
            "FILTER (!bound(?imAccount)).");

    subBuilder = CDTpQueryBuilder("SyncRosterAccounts - purge contacts");
    subBuilder.appendRawSelection(selection);
    addRemoveContactToBuilder(subBuilder, imAddressVar);
    builder.appendRawQuery(subBuilder);

    /* FIXME: We leak avatar object */
    subBuilder = CDTpQueryBuilder("SyncRosterAccounts - purge imAddresses");
    subBuilder.appendRawSelection(selection);
    subBuilder.deleteResource(imAddressVar);
    builder.appendRawQuery(subBuilder);
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
        builder.insertProperty(imAccount, "nco:hasIMContact", imAddress, privateGraph);

        // Add mutable properties except for ContactInfo
        addContactChangesToBuilder(builder, imAddress, CDTpContact::All,
                contactWrapper->contact());

        imAddresses << imAddress;
    }

    // Ensure all imAddresses are bound to a PersonContact
    static const QString tmpl = QString::fromLatin1(
        "INSERT {\n"
        "    GRAPH %1 {\n"
        "        _:contact a nco:PersonContact;\n"
        "                  nco:hasAffiliation _:affiliation;\n"
        "                  nie:contentCreated %2;\n"
        "                  nie:generator %3 .\n"
        "        _:affiliation a nco:Affiliation;\n"
        "                      nco:hasIMAddress ?imAddress;\n"
        "                      rdfs:label \"Other\".\n"
        "    }\n"
        "}\n"
        "WHERE {\n"
        "    ?imAddress a nco:IMAddress.\n"
        "    FILTER (NOT EXISTS { ?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ] })\n"
        "}\n");
    builder.appendRawQuery(tmpl.arg(CDTpQueryBuilder::defaultGraph).arg(literalTimeStamp()).arg(defaultGenerator));

    // Insert timestamp on all imContact bound to those imAddresses
    CDTpQueryBuilder subBuilder("CreateContacts - timestamp");
    static const QString tmpl2 = QString::fromLatin1(
            "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
            "FILTER(?imAddress IN (%1)).");
    subBuilder.appendRawSelection(tmpl2.arg(imAddresses.join(filterInSeparator)));
    subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
    builder.appendRawQuery(subBuilder);

    // Add ContactInfo seperately because we need to bind to the PersonContact
    // and that makes things much slower
    subBuilder = CDTpQueryBuilder("CreateContacts - add ContactInfo");
    uint imContactCount = 0;
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        if (!contactWrapper->isInformationKnown()) {
            continue;
        }

        const QString imAddress = literalIMAddress(contactWrapper);

        // Tracker supports at most 32 selection of the ?imContact_X
        if (imContactCount == 32) {
            builder.appendRawQuery(subBuilder);
            subBuilder = CDTpQueryBuilder("CreateContacts - add ContactInfo - next part");
            imContactCount = 0;
        }

        imContactCount++;
        const QString imContact = subBuilder.uniquify("?imContact");
        static const QString tmpl = QString::fromLatin1("%1 nco:hasAffiliation [ nco:hasIMAddress %2 ].");
        subBuilder.appendRawSelection(tmpl.arg(imContact).arg(imAddress));
        addContactInfoToBuilder(subBuilder, imAddress, imContact,
                contactWrapper->contact());
    }
    builder.appendRawQuery(subBuilder);
}

void CDTpStorage::addRemoveContactsChangesToBuilder(CDTpQueryBuilder &builder,
    const QList<CDTpAccountPtr> &accounts) const
{
    QStringList imAccounts;
    QStringList imAddresses;
    QList<CDTpContactPtr> allContacts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        imAccounts << literalIMAccount(accountWrapper);
        imAddresses << literalIMAddress(accountWrapper);
        allContacts << accountWrapper->contacts();
    }

    if (allContacts.isEmpty()) {
        return;
    }

    /* We can easily drop all mutable properties at once for all contacts of
     * all those accounts. BUT Avatar and ContactInfo are special because they
     * could not be known yet in which case we want to keep the current value.
     * so we delete Avatar/Information later only for the contacts that already
     * have it. */

    uint mandatoryFlags = CDTpContact::All;
    mandatoryFlags &= ~CDTpContact::Information;
    mandatoryFlags &= ~CDTpContact::Avatar;
    static const QString tmpl = QString::fromLatin1(
            "?imAccount nco:hasIMContact ?imAddress.\n"
            "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
            "FILTER(?imAddress NOT IN (%1) && ?imAccount IN (%2)).");
    builder.appendRawSelection(tmpl.arg(imAddresses.join(filterInSeparator)).arg(imAccounts.join(filterInSeparator)));
    addRemoveContactsChangesToBuilder(builder, imAddressVar, imContactVar, CDTpContact::Changes(mandatoryFlags));

    QStringList avatarIMAddresses;
    QStringList infoIMAddresses;
    Q_FOREACH (const CDTpContactPtr contactWrapper, allContacts) {
        if (contactWrapper->isAvatarKnown()) {
            avatarIMAddresses << literalIMAddress(contactWrapper);
        }
        if (contactWrapper->isInformationKnown()) {
            infoIMAddresses << literalIMAddress(contactWrapper);
        }
    }

    CDTpQueryBuilder subBuilder;

    /* Delete Avatar for those who know it */
    if (!avatarIMAddresses.isEmpty()) {
        subBuilder = CDTpQueryBuilder("SyncContacts - remove known Avatar");
        static const QString tmpl = QString::fromLatin1(
                "?imAddress a nco:IMAddress.\n"
                "FILTER(?imAddress IN (%1))");
        subBuilder.appendRawSelection(tmpl.arg(avatarIMAddresses.join(filterInSeparator)));
        addRemoveAvatarToBuilder(subBuilder, imAddressVar);
        builder.appendRawQuery(subBuilder);
    }

    /* Delete ContactInfo for those who know it */
    if (!infoIMAddresses.isEmpty()) {
        subBuilder = CDTpQueryBuilder("SyncContacts - remove known ContactInfo");
        static const QString tmpl = QString::fromLatin1(
                "?imContact nco:hasAffiliation [ nco:hasIMAddress ?imAddress ].\n"
                "FILTER(?imAddress IN (%1))");
        subBuilder.appendRawSelection(tmpl.arg(infoIMAddresses.join(filterInSeparator)));
        addRemoveContactInfoToBuilder(builder, imAddressVar, imContactVar);
        builder.appendRawQuery(subBuilder);
    }
}

void CDTpStorage::addRemoveContactsChangesToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &imContact,
        CDTpContact::Changes changes) const
{
    if (changes & CDTpContact::Alias) {
        debug() << "  alias changed";
        builder.deleteProperty(imAddress, "nco:imNickname");
    }
    if (changes & CDTpContact::Presence) {
        debug() << "  presence changed";
        addRemovePresenceToBuilder(builder, imAddress);
    }
    if (changes & CDTpContact::Capabilities) {
        debug() << "  capabilities changed";
        addRemoveCapabilitiesToBuilder(builder, imAddress);
    }
    if (changes & CDTpContact::Avatar) {
        debug() << "  avatar changed";
        addRemoveAvatarToBuilder(builder, imAddress);
    }
    if (changes & CDTpContact::Authorization) {
        debug() << "  authorization changed";
        builder.deleteProperty(imAddress, "nco:imAddressAuthStatusFrom");
        builder.deleteProperty(imAddress, "nco:imAddressAuthStatusTo");
    }
    if (changes & CDTpContact::Information) {
        debug() << "  vcard information changed";
        addRemoveContactInfoToBuilder(builder, imAddress, imContact);
    }

    builder.deleteProperty(imContact, "nie:contentLastModified");
}

void CDTpStorage::addContactChangesToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &imContact,
        CDTpContact::Changes changes,
        Tp::ContactPtr contact) const
{
    // Apply changes
    addContactChangesToBuilder(builder, imAddress, changes, contact);
    if (changes & CDTpContact::Information) {
        debug() << "  vcard information changed";
        addContactInfoToBuilder(builder, imAddress, imContact, contact);
    }

    builder.insertProperty(imContact, "nie:contentLastModified", literalTimeStamp());
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
        builder.insertProperty(imAddress, "nco:imNickname", literal(contact->alias()));
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
        builder.insertProperty(imAddress, "nco:imAddressAuthStatusFrom", presenceState(contact->subscriptionState()));
        builder.insertProperty(imAddress, "nco:imAddressAuthStatusTo", presenceState(contact->publishState()));
    }
}

void CDTpStorage::addRemovePresenceToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress) const
{
    builder.deleteProperty(imAddress, "nco:imPresence");
    builder.deleteProperty(imAddress, "nco:imStatusMessage");
    builder.deleteProperty(imAddress, "nco:presenceLastModified");
}

void CDTpStorage::addPresenceToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const Tp::Presence &presence) const
{
    builder.insertProperty(imAddress, "nco:imPresence", presenceType(presence.type()));
    builder.insertProperty(imAddress, "nco:presenceLastModified", literalTimeStamp());
    if (!presence.statusMessage().isEmpty()) {
        builder.insertProperty(imAddress, "nco:imStatusMessage", literal(presence.statusMessage()));
    }
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
        builder.insertProperty(imAddress, "nco:imCapability", value);
    }
    if (capabilities.streamedMediaAudioCalls()) {
        static const QString value = QString::fromLatin1("nco:im-capability-audio-calls");
        builder.insertProperty(imAddress, "nco:imCapability", value);
    }
    if (capabilities.streamedMediaVideoCalls()) {
        static const QString value = QString::fromLatin1("nco:im-capability-video-calls");
        builder.insertProperty(imAddress, "nco:imCapability", value);
    }
}

void CDTpStorage::addRemoveAvatarToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress) const
{
    builder.deletePropertyAndLinkedResource(imAddress, "nco:imAvatar");
}

void CDTpStorage::addAvatarToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &fileName) const
{
    if (!fileName.isEmpty()) {
        const QString dataObject = builder.uniquify("_:dataObject");
        builder.createResource(dataObject, "nfo:FileDataObject");
        builder.insertProperty(dataObject, "nie:url", literal(QUrl::fromLocalFile(fileName).toString()));
        builder.insertProperty(imAddress, "nco:imAvatar", dataObject);
    }
}

void CDTpStorage::addContactInfoToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &imContact,
        Tp::ContactPtr contact) const
{
    /* Use the imAddress as graph for ContactInfo fields, so we can easilly
     * know from which contact it comes from */
    const QString graph = imAddress;

    Tp::ContactInfoFieldList listContactInfo = contact->infoFields().allFields();
    if (listContactInfo.count() == 0) {
        debug() << "No contact info present";
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

        if (!field.fieldName.compare(QLatin1String("tel"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            const QString voicePhoneNumber = builder.uniquify("_:tel");
            builder.createResource(voicePhoneNumber, "nco:VoicePhoneNumber", graph);
            builder.insertProperty(voicePhoneNumber, "maemo:localPhoneNumber", literalContactInfo(field, 0), graph);
            builder.insertProperty(voicePhoneNumber, "nco:phoneNumber", literalContactInfo(field, 0), graph);
            builder.insertProperty(affiliation, "nco:hasPhoneNumber", voicePhoneNumber, graph);
        }

        else if (!field.fieldName.compare(QLatin1String("adr"))) {
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

        else if (!field.fieldName.compare(QLatin1String("email"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            const QString emailAddress = builder.uniquify("_:email");
            builder.createResource(emailAddress, "nco:EmailAddress", graph);
            builder.insertProperty(emailAddress, "nco:emailAddress", literalContactInfo(field, 0), graph);
            builder.insertProperty(affiliation, "nco:hasEmailAddress", emailAddress, graph);
        }

        else if (!field.fieldName.compare(QLatin1String("url"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:url", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare(QLatin1String("title"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:title", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare(QLatin1String("role"))) {
            const QString affiliation = ensureContactAffiliationToBuilder(builder, imContact, graph, field, affiliations);
            builder.insertProperty(affiliation, "nco:role", literalContactInfo(field, 0), graph);
        }

        else if (!field.fieldName.compare(QLatin1String("note")) || !field.fieldName.compare(QLatin1String("desc"))) {
            builder.insertProperty(imContact, "nco:note", literalContactInfo(field, 0), graph);
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
                builder.insertProperty(imContact, "nco:birthDate", literal(QDateTime(date)), graph);
            } else {
                debug() << "Unsupported bday format:" << field.fieldValue[0];
            }
        }

        else {
            debug() << "Unsupported VCard field" << field.fieldName;
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
    QStringList contactIds;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        contactIds << contactWrapper->contact()->id();
    }

    addRemoveContactsToBuilder(builder, accountWrapper->account()->objectPath(),
            contactIds);
}

void CDTpStorage::addRemoveContactsToBuilder(CDTpQueryBuilder &builder,
        const QString &accountPath,
        const QStringList &contactIds) const
{
    QStringList imAddresses;
    Q_FOREACH (const QString &contactId, contactIds) {
        imAddresses << literalIMAddress(accountPath, contactId);
    }

    const QString imAccount = literalIMAccount(accountPath);

    /* Bind imAddress to all those contacts */
    static const QString tmpl = QString::fromLatin1(
            "?imAddress a nco:IMAddress.\n"
            "FILTER (?imAddress IN (%1)).");
    const QString selection = tmpl.arg(imAddresses.join(filterInSeparator));

    builder.appendRawSelection(selection);
    addRemoveContactToBuilder(builder, imAddressVar);

    CDTpQueryBuilder subBuilder("RemoveContacts - imAddresses");
    subBuilder.appendRawSelection(selection);
    subBuilder.deleteProperty(imAccount, "nco:hasIMContact", imAddressVar);
    subBuilder.deleteResource(imAddressVar);
    builder.appendRawQuery(subBuilder);
}

void CDTpStorage::addRemoveContactToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress) const
{
    /* Clean nco:PersonContact from all info imported from the imAddress */
    static const QString affiliation = QString::fromLatin1("?affiliation");
    static const QString tmpl = QString::fromLatin1(
            "?imContact nco:hasAffiliation ?affiliation.\n"
            "?affiliation nco:hasIMAddress %1.");
    builder.appendRawSelection(tmpl.arg(imAddress));
    builder.deleteProperty(imContactVar, "nie:contentLastModified");
    builder.deleteProperty(imContactVar, "nco:hasAffiliation", affiliation);
    addRemoveContactInfoToBuilder(builder, imAddress, imContactVar);
    builder.deleteResource(affiliation);

    /* Purge nco:PersonContact with generator "telepathy" but with no
     * nco:IMAddress bound anymore */
    CDTpQueryBuilder subBuilder("RemoveContact - purge");
    static const QString tmpl2 = QString::fromLatin1(
            "?imContact nie:generator %1.\n"
            "FILTER(NOT EXISTS { ?imContact nco:hasAffiliation [nco:hasIMAddress ?v] })")
            .arg(defaultGenerator);
    subBuilder.appendRawSelection(tmpl2);
    subBuilder.deleteResource(imContactVar);
    builder.appendRawQuery(subBuilder);

    /* We deleted the affiliation, so we can't know anymore for which imContact
     * we need to add new value for contentLastModified. So we do for all
     * contacts missing that field */
    subBuilder = CDTpQueryBuilder("RemoveContact - add timestamp");
    static const QString tmpl3 = QString::fromLatin1(
            "?imContact a nco:PersonContact.\n"
            "FILTER(NOT EXISTS { ?imContact nie:contentLastModified ?v }).");
    subBuilder.appendRawSelection(tmpl3);
    subBuilder.insertProperty(imContactVar, "nie:contentLastModified", literalTimeStamp());
    builder.appendRawQuery(subBuilder);
}

void CDTpStorage::addRemoveContactInfoToBuilder(CDTpQueryBuilder &builder,
        const QString &imAddress,
        const QString &imContact) const
{
    /* imAddress is used as graph for properties on the imContact */
    const QString graph = imAddress;

    builder.deletePropertyWithGraph(imContact, "nco:birthDate", graph);
    builder.deletePropertyWithGraph(imContact, "nco:note", graph);

    /* Remove affiliation and its sub resources */
    const QString affiliation = builder.deletePropertyWithGraph(imContact, "nco:hasAffiliation", graph);
    builder.deletePropertyAndLinkedResource(affiliation, "nco:hasPhoneNumber", graph);
    builder.deletePropertyAndLinkedResource(affiliation, "nco:hasPostalAddress", graph);
    builder.deletePropertyAndLinkedResource(affiliation, "nco:emailAddress", graph);
    builder.deleteResource(affiliation);
}

void CDTpStorage::onSyncOperationEnded(CDTpSparqlQuery *query)
{
    CDTpAccountsSparqlQuery *accountsQuery = qobject_cast<CDTpAccountsSparqlQuery*>(query);
    QList<CDTpAccountPtr> accounts = accountsQuery->accounts();

    /* FIXME: We don't know how many contacts were imported and how many just
     * got updated */
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

QString CDTpStorage::literalIMAddress(const QString &accountPath, const QString &contactId) const
{
    // FIXME: we can't use QUrl::toPercentEncoding() here because the string
    // will be used in some QString::arg().
    static const QString tmpl = QString::fromLatin1("<telepathy:%1!%2>");
    return tmpl.arg(accountPath).arg(QString(contactId).remove(QLatin1Char('<')).remove(QLatin1Char('>')));
}

QString CDTpStorage::literalIMAddress(const CDTpContactPtr &contactWrapper) const
{
    const QString accountPath = contactWrapper->accountWrapper()->account()->objectPath();
    const QString contactId = contactWrapper->contact()->id();
    return literalIMAddress(accountPath, contactId);
}

QString CDTpStorage::literalIMAddress(const CDTpAccountPtr &accountWrapper) const
{
    static const QString tmpl = QString::fromLatin1("<telepathy:%1!self>");
    return tmpl.arg(accountWrapper->account()->objectPath());
}

QString CDTpStorage::literalIMAccount(const QString &accountPath) const
{
    static const QString tmpl = QString::fromLatin1("<telepathy:%1>");
    return tmpl.arg(accountPath);
}

QString CDTpStorage::literalIMAccount(const CDTpAccountPtr &accountWrapper) const
{
    return literalIMAccount(accountWrapper->account()->objectPath());
}

QString CDTpStorage::literalContactInfo(const Tp::ContactInfoField &field, int i) const
{
    if (i >= field.fieldValue.count()) {
        return literal(QString());
    }

    return literal(field.fieldValue[i]);
}

