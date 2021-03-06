/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 ** Copyright (c) 2012 - 2019 Jolla Ltd.
 ** Copyright (c) 2020 Open Mobile Platform LLC.
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#include <TelepathyQt/AvatarData>
#include <TelepathyQt/ContactCapabilities>
#include <TelepathyQt/ContactManager>
#include <TelepathyQt/ConnectionCapabilities>

#include <qtcontacts-extensions.h>
#include <qtcontacts-extensions_manager_impl.h>
#include <contactmanagerengine.h>
#include <QContactOriginMetadata>

#include <seasidecache.h>

#include <QContact>
#include <QContactManager>
#include <QContactDetail>
#include <QContactDetailFilter>
#include <QContactIntersectionFilter>
#include <QContactRelationshipFilter>
#include <QContactUnionFilter>
#include <QContactIdFilter>
#include <QContactCollectionFilter>

#include <QContactAddress>
#include <QContactAvatar>
#include <QContactBirthday>
#include <QContactDisplayLabel>
#include <QContactEmailAddress>
#include <QContactGender>
#include <QContactGlobalPresence>
#include <QContactName>
#include <QContactNickname>
#include <QContactNote>
#include <QContactOnlineAccount>
#include <QContactOrganization>
#include <QContactPhoneNumber>
#include <QContactPresence>
#include <QContactRelationship>
#include <QContactUrl>

#include "cdtpstorage.h"
#include "cdtpavatarupdate.h"
#include "cdtpplugin.h"
#include "cdtpdevicepresence.h"
#include "debug.h"

#include <QElapsedTimer>

using namespace Contactsd;

// Uncomment for masses of debug output:
//#define DEBUG_OVERLOAD

// The longer a single batch takes to write, the longer we are locking out other
// writers (readers should be unaffected).  Using a semaphore write mutex, we should
// at least have FIFO semantics on lock release.
#define BATCH_STORE_SIZE 5

typedef QList<QContactDetail::DetailType> DetailList;

namespace {

const QString telepathyCollectionName = QStringLiteral("telepathy");

template<int N>
const QString &sourceLocation(const char *f)
{
    static const QString tmpl(QString::fromLatin1("%2:%1").arg(N));
    static const QString loc(tmpl.arg(QString::fromLatin1(f)));
    return loc;
}

#define SRC_LOC sourceLocation<__LINE__>(__PRETTY_FUNCTION__)

QString asString(bool f)
{
    return QLatin1String(f ? "true" : "false");
}

QString asString(const Tp::ContactInfoField &field, int i)
{
    if (i >= field.fieldValue.count()) {
        return QLatin1String("");
    }

    return field.fieldValue[i];
}

QStringList asStringList(const Tp::ContactInfoField &field, int i)
{
    QStringList rv;

    while (i < field.fieldValue.count()) {
        rv.append(field.fieldValue[i].trimmed());
        ++i;
    }

    return rv;
}

QString asString(CDTpContact::Info::Capability c)
{
    switch (c) {
        case CDTpContact::Info::TextChats:
            return QLatin1String("TextChats");
        case CDTpContact::Info::StreamedMediaCalls:
            return QLatin1String("StreamedMediaCalls");
        case CDTpContact::Info::StreamedMediaAudioCalls:
            return QLatin1String("StreamedMediaAudioCalls");
        case CDTpContact::Info::StreamedMediaAudioVideoCalls:
            return QLatin1String("StreamedMediaAudioVideoCalls");
        case CDTpContact::Info::UpgradingStreamMediaCalls:
            return QLatin1String("UpgradingStreamMediaCalls");
        case CDTpContact::Info::FileTransfers:
            return QLatin1String("FileTransfers");
        case CDTpContact::Info::StreamTubes:
            return QLatin1String("StreamTubes");
        case CDTpContact::Info::DBusTubes:
            return QLatin1String("DBusTubes");
        default:
            break;
    }

    return QString();
}

QString asString(CDTpAccount::Changes changes)
{
    QStringList rv;
    if (changes & CDTpAccount::DisplayName) { rv.append(QLatin1String("DisplayName")); }
    if (changes & CDTpAccount::Nickname) { rv.append(QLatin1String("Nickname")); }
    if (changes & CDTpAccount::Presence) { rv.append(QLatin1String("Presence")); }
    if (changes & CDTpAccount::Avatar) { rv.append(QLatin1String("Avatar")); }
    if (changes & CDTpAccount::Enabled) { rv.append(QLatin1String("Enabled")); }
    if (changes & CDTpAccount::StorageInfo) { rv.append(QLatin1String("StorageInfo")); }
    return rv.join(QLatin1Char(':'));
}

QString asString(const QContactId &id) { return id.toString(); }

}

template<typename F>
QString stringValue(const QContactDetail &detail, F field)
{
    return detail.value<QString>(field);
}

namespace {

const int UPDATE_TIMEOUT = 250; // ms
const int UPDATE_MAXIMUM_TIMEOUT = 2000; // ms

QMap<QString, QString> managerParameters()
{
    QMap<QString, QString> parameters;
    parameters.insert(QString::fromLatin1("mergePresenceChanges"), QString::fromLatin1("false"));
    return parameters;
}

QContactManager *manager()
{
    static QContactManager manager(QStringLiteral("org.nemomobile.contacts.sqlite"), managerParameters());
    return &manager;
}

template<typename T>
DetailList::value_type detailType()
{
    return T::Type;
}

bool matchesTelepathyCollectionId(const QContactCollection &collection, int matchAccountId = 0)
{
    const QString name = collection.metaData(QContactCollection::KeyName).toString();
    const QString appName = collection.extendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_APPLICATIONNAME).toString();
    const int accountId = collection.extendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_ACCOUNTID).toInt();

    return name == telepathyCollectionName
            && appName == QCoreApplication::applicationName()
            && (matchAccountId == 0 || matchAccountId == accountId);
}

QList<QContactCollection> allTelepathyCollections()
{
    QList<QContactCollection> telepathyCollections;

    const QList<QContactCollection> collections = manager()->collections();
    for (const QContactCollection &collection : collections) {
        if (matchesTelepathyCollectionId(collection)) {
            qCDebug(lcContactsd) << "Found telepathy collection" << collection.id();
            telepathyCollections.append(collection);
        }
    }

    return telepathyCollections;
}

QContactCollectionId telepathyCollectionId(int accountId)
{
    const QList<QContactCollection> collections = manager()->collections();
    QContactCollectionId matchedCollectionId;

    for (const QContactCollection &collection : collections) {
        if (matchesTelepathyCollectionId(collection, accountId)) {
            qCDebug(lcContactsd) << "Found telepathy collection" << collection.id()
                    << "for accountId:" << accountId;
            matchedCollectionId = collection.id();
            break;
        }
    }

    if (!matchedCollectionId.isNull()) {
        return matchedCollectionId;
    }

    QContactCollection collection;
    collection.setMetaData(QContactCollection::KeyName, telepathyCollectionName);
    collection.setMetaData(QContactCollection::KeyDescription, QStringLiteral("Telepathy contacts"));
    collection.setMetaData(QContactCollection::KeyColor, QStringLiteral("darkblue"));
    collection.setMetaData(QContactCollection::KeySecondaryColor, QStringLiteral("blue"));
    collection.setMetaData(QContactCollection::KeyImage, QStringLiteral("image://theme/graphic-service-xmpp"));
    collection.setExtendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_APPLICATIONNAME, QCoreApplication::applicationName());
    collection.setExtendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_READONLY, true);
    collection.setExtendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_ACCOUNTID, accountId);

    if (manager()->saveCollection(&collection)) {
        qCDebug(lcContactsd) << "Created telepathy collection for account:" << accountId;
        return collection.id();
    }

    qCWarning(lcContactsd) << "Unable to create telepathy collection for account!" << accountId
              << "error was:" << manager()->error();
    return QContactCollectionId();
}

QContactCollectionId telepathyCollectionId(const QString &accountPath)
{
    const int i = accountPath.lastIndexOf(QLatin1Char('_'));
    if (i >= 0) {
        int accountId = accountPath.mid(i + 1).toInt();
        if (accountId > 0) {
            return telepathyCollectionId(accountId);
        }
    }

    qCWarning(lcContactsd) << "telepathy accountPath does not contain valid account id:" << accountPath;
    return QContactCollectionId();
}

QContactFetchHint contactFetchHint(const DetailList &detailTypes = DetailList())
{
    QContactFetchHint hint;

    // Relationships are slow and unnecessary here:
    hint.setOptimizationHints(QContactFetchHint::NoRelationships |
                              QContactFetchHint::NoActionPreferences |
                              QContactFetchHint::NoBinaryBlobs);

    if (!detailTypes.isEmpty()) {
        hint.setDetailTypesHint(detailTypes);
    }

    return hint;
}

QContactId selfContactAggregateId()
{
    return manager()->selfContactId();
}

QContactId selfContactLocalId(const QContactCollectionId &collectionId)
{
    QContactManager *mgr(manager());

    // Check that there is a self contact
    QContactId selfId(selfContactAggregateId());

    // Find the telepathy contact aggregated by the real self contact
    QContactRelationshipFilter relationshipFilter;
    relationshipFilter.setRelationshipType(QContactRelationship::Aggregates());
    QContact relatedContact;
    relatedContact.setId(selfId);
    relationshipFilter.setRelatedContactId(relatedContact.id());
    relationshipFilter.setRelatedContactRole(QContactRelationship::First);

    QContactCollectionFilter collectionFilter;
    collectionFilter.setCollectionId(collectionId);

    QContactIntersectionFilter selfFilter;
    selfFilter << collectionFilter;
    selfFilter << relationshipFilter;

    QList<QContactId> selfContactIds = mgr->contactIds(selfFilter);
    if (selfContactIds.count() > 0) {
        if (selfContactIds.count() > 1) {
            qCWarning(lcContactsd) << "Invalid number of telepathy self contacts!" << selfContactIds.count();
        }
        qCDebug(lcContactsd) << "Found self contact" << selfContactIds.first() << "for collection:" << collectionId;
        return selfContactIds.first();
    }

    // Create a new self contact for telepathy
    qCDebug(lcContactsd) << "Creating self contact for collection:" << collectionId;
    QContact tpSelf;
    tpSelf.setCollectionId(collectionId);

    if (!mgr->saveContact(&tpSelf)) {
        qCWarning(lcContactsd) << "Unable to save empty contact as self contact - error:" << mgr->error();
    } else {
        // Now connect our contact to the real self contact
        QContactRelationship relationship;
        relationship.setRelationshipType(QContactRelationship::Aggregates());
        relatedContact.setId(selfId);
        relationship.setFirst(relatedContact.id());
        relationship.setSecond(tpSelf.id());

        if (!mgr->saveRelationship(&relationship)) {
            qCWarning(lcContactsd) << "Unable to save relationship for self contact - error:" << mgr->error();
            qFatal("Cannot proceed with invalid self contact!");
        }

        // Find the aggregate contact created by saving our self contact
        relationshipFilter.setRelationshipType(QContactRelationship::Aggregates());
        relatedContact.setId(tpSelf.id());
        relationshipFilter.setRelatedContactId(relatedContact.id());
        relationshipFilter.setRelatedContactRole(QContactRelationship::Second);

        foreach (const QContact &aggregator, mgr->contacts(relationshipFilter)) {
            if (aggregator.id() == tpSelf.id())
                continue;

            // Remove the relationship between these contacts (which removes the childless aggregate)
            QContactRelationship relationship;
            relationship.setRelationshipType(QContactRelationship::Aggregates());
            relationship.setFirst(aggregator.id());
            relationship.setSecond(tpSelf.id());

            if (!mgr->removeRelationship(relationship)) {
                qCWarning(lcContactsd) << "Unable to remove relationship for self contact - error:" << mgr->error();
            }
        }

        return tpSelf.id();
    }

    return QContactId();
}

QContact selfContact(const QContactCollectionId &collectionId)
{
    // For the self contact, we only care about accounts/presence/avatars
    QContactId selfLocalId(selfContactLocalId(collectionId));
    static QContactFetchHint hint(contactFetchHint(DetailList() << detailType<QContactOnlineAccount>()
                                                                << detailType<QContactPresence>()
                                                                << detailType<QContactGlobalPresence>()
                                                                << detailType<QContactAvatar>()));

    return manager()->contact(selfLocalId, hint);
}

QContact selfDetailsContact()
{
    static QContactFetchHint hint(contactFetchHint(DetailList() << detailType<QContactName>()
                                                                << detailType<QContactDisplayLabel>()
                                                                << detailType<QContactNickname>()));
    return manager()->contact(selfContactAggregateId(), hint);
}

static void output(const QContactDetail &detail)
{
    const QMap<int, QVariant> &values(detail.values());
    QMap<int, QVariant>::const_iterator it = values.constBegin(), end = values.constEnd();
    for ( ; it != end; ++it) {
        qCDebug(lcContactsd) << "   -" << it.key() << ":" << it.value();
    }
}

QContactDetail::DetailType detailType(const QContactDetail &detail) { return detail.type(); }

static void output(const QContact &contact)
{
    const QList<QContactDetail> &details(contact.details());
    foreach (const QContactDetail &detail, details) {
        qCDebug(lcContactsd) << "  Detail:" << detailType(detail);
        output(detail);
    }
}

bool storeContactDetail(QContact &contact, QContactDetail &detail, const QString &location)
{
#ifdef DEBUG_OVERLOAD
    qCDebug(lcContactsd) << "  Storing" << detailType(detail) << "from:" << location;
    output(detail);
#endif

    if (!contact.saveDetail(&detail)) {
        qCDebug(lcContactsd) << "  Failed storing" << detailType(detail) << "from:" << location;
#ifndef DEBUG_OVERLOAD
        output(detail);
#endif
        return false;
    }
    return true;
}

DetailList contactChangesList(CDTpContact::Changes changes)
{
    DetailList rv;

    if ((changes & CDTpContact::Information) == 0) {
        if (changes & CDTpContact::Alias) {
            rv.append(detailType<QContactNickname>());
        }
        if (changes & CDTpContact::Presence) {
            rv.append(detailType<QContactPresence>());
            rv.append(detailType<QContactGlobalPresence>());
        }
        if (changes & CDTpContact::Capabilities) {
            rv.append(detailType<QContactOnlineAccount>());
            rv.append(detailType<QContactOriginMetadata>());
        }
        if (changes & CDTpContact::Avatar) {
            rv.append(detailType<QContactAvatar>());
        }
    }

    return rv;
}

bool storeContact(QContact &contact, const QString &location, CDTpContact::Changes changes = CDTpContact::All)
{
    const DetailList updates = contactChangesList(changes);
    const bool minimizedUpdate(!updates.isEmpty());

#ifdef DEBUG_OVERLOAD
    qCDebug(lcContactsd) << "Storing contact" << asString(contact.id()) << "from:" << location;
    output(contact);
#endif

    if (minimizedUpdate) {
        QList<QContact> contacts;
        contacts << contact;
        if (!manager()->saveContacts(&contacts, updates)) {
            qCWarning(lcContactsd) << "Failed minimized storing contact" << asString(contact.id()) << "from:" << location << "error:" << manager()->error();
#ifndef DEBUG_OVERLOAD
            output(contact);
#endif
            qCDebug(lcContactsd) << "Updates" << updates;
            return false;
        }
    } else {
        if (!manager()->saveContact(&contact)) {
            qCWarning(lcContactsd) << "Failed storing contact" << asString(contact.id()) << "from:" << location;
#ifndef DEBUG_OVERLOAD
            output(contact);
#endif
            return false;
        }
    }
    return true;
}

QChar::Script nameScript(const QString &name)
{
    QChar::Script script(QChar::Script_Unknown);

    if (!name.isEmpty()) {
        QString::const_iterator it = name.begin(), end = name.end();
        for ( ; it != end; ++it) {
            const QChar::Category charCategory((*it).category());
            if (charCategory >= QChar::Letter_Uppercase && charCategory <= QChar::Letter_Other) {
                const QChar::Script charScript((*it).script());
                if (script == QChar::Script_Unknown) {
                    script = charScript;
                } else if (charScript != script) {
                    return QChar::Script_Unknown;
                }
            }
        }
    }

    return script;
}

QChar::Script nameScript(const QString &firstName, const QString &lastName)
{
    if (firstName.isEmpty()) {
        return nameScript(lastName);
    } else if (lastName.isEmpty()) {
        return nameScript(firstName);
    }

    QChar::Script firstScript(nameScript(firstName));
    if (firstScript != QChar::Script_Unknown) {
        QChar::Script lastScript(nameScript(lastName));
        if (lastScript == firstScript) {
            return lastScript;
        }
    }

    return QChar::Script_Unknown;
}

bool nameScriptImpliesFamilyFirst(const QString &firstName, const QString &lastName)
{
    switch (nameScript(firstName, lastName)) {
        // These scripts are used by cultures that conform to the family-name-first naming convention:
        case QChar::Script_Han:
        case QChar::Script_Lao:
        case QChar::Script_Hangul:
        case QChar::Script_Khmer:
        case QChar::Script_Mongolian:
        case QChar::Script_Hiragana:
        case QChar::Script_Katakana:
        case QChar::Script_Bopomofo:
        case QChar::Script_Yi:
            return true;
        default:
            return false;
    }
}

bool needsSpaceBetweenNames(const QString &first, const QString &second)
{
    if (first.isEmpty() || second.isEmpty()) {
        return false;
    }
    return first[first.length()-1].script() != QChar::Script_Han
            || second[0].script() != QChar::Script_Han;
}

QString generateDisplayLabel(const QContactName &nameDetail, CDTpStorage::DisplayLabelOrder order)
{
    // Simplified version of the SeasideCache displayLabel generator
    QString rv;

    QString nameStr1(nameDetail.firstName());
    QString nameStr2(nameDetail.lastName());

    const bool familyNameFirst(order == CDTpStorage::LastNameFirst || nameScriptImpliesFamilyFirst(nameStr1, nameStr2));
    if (familyNameFirst) {
        nameStr1 = nameDetail.lastName();
        nameStr2 = nameDetail.firstName();
    }

    if (!nameStr1.isEmpty())
        rv.append(nameStr1);

    if (!nameStr2.isEmpty()) {
        if (needsSpaceBetweenNames(nameStr1, nameStr2)) {
            rv.append(QLatin1Char(' '));
        }
        rv.append(nameStr2);
    }

    return rv;
}

void reportSelfDetails(CDTpDevicePresence *devicePresence, const QContact &contact, CDTpStorage::DisplayLabelOrder order)
{
    const QContactName nameDetail(contact.detail<QContactName>());

    QStringList nicknames;
    foreach (const QContactNickname &nickname, contact.details<QContactNickname>()) {
        nicknames.append(nickname.nickname());
    }

    QString displayLabel(generateDisplayLabel(nameDetail, order));
    if (displayLabel.isEmpty()) {
        displayLabel = contact.detail<QContactDisplayLabel>().label();
    }

    emit devicePresence->selfUpdate(displayLabel, nameDetail.firstName(), nameDetail.lastName(), nicknames);
}

void emitAccountChanges(CDTpDevicePresence *devicePresence, QContact &self, bool updateAccountList = false)
{
    // See if the global presence has been updated
    const QContactPresence::PresenceState previousState(self.detail<QContactGlobalPresence>().presenceState());

    DetailList types(DetailList() << detailType<QContactGlobalPresence>());
    if (updateAccountList) {
        types << detailType<QContactOnlineAccount>();
    }

    const QContact updated(manager()->contact(self.id(), contactFetchHint(types)));
    const QContactPresence::PresenceState updatedState(updated.detail<QContactGlobalPresence>().presenceState());

    if (updatedState != previousState) {
        emit devicePresence->globalUpdate(updatedState);
    }
    if (updateAccountList) {
        // Ensure that listeners are aware of any invalidated accounts
        QStringList accountPaths;
        foreach (const QContactOnlineAccount &qcoa, updated.details<QContactOnlineAccount>()) {
            accountPaths.append(qcoa.value<QString>(QContactOnlineAccount__FieldAccountPath));
        }
        emit devicePresence->accountList(accountPaths);
    }
}

bool storeSelfContact(CDTpDevicePresence *devicePresence, QContact &self, const QString &location, CDTpContact::Changes changes = CDTpContact::All, bool updateAccountList = false)
{
    if (!storeContact(self, location, changes)) {
        return false;
    }

    if ((changes & CDTpContact::Presence) || updateAccountList) {
        emitAccountChanges(devicePresence, self, updateAccountList);
    }
    return true;
}

void appendContactChange(CDTpStorage::ContactChangeSet *saveSet, const QContact &contact, CDTpContact::Changes changes)
{
    if (changes != 0) {
        if (changes & CDTpContact::Information) {
            // All changes including Information will be full stores, so group them all together
            changes = CDTpContact::All;
        }
        (*saveSet)[changes].append(contact);
    }
}

void updateContacts(const QString &location, CDTpStorage::ContactChangeSet *saveSet, QList<QContactId> *removeList)
{
    if (saveSet && !saveSet->isEmpty()) {
        // Each element of the save set is a list of contacts with the same set of changes
        CDTpStorage::ContactChangeSet::iterator sit = saveSet->begin(), send = saveSet->end();
        for ( ; sit != send; ++sit) {
            CDTpContact::Changes changes = sit.key();
            QList<QContact> *saveList = &(sit.value());

            if (saveList && !saveList->isEmpty()) {
                // Restrict the update to only modify the detail types that have changed for these contacts
                const DetailList detailList(contactChangesList(changes));

                QElapsedTimer t;
                t.start();

                // Try to store contacts in batches
                int storedCount = 0;
                while (storedCount < saveList->count()) {
                    QList<QContact> batch(saveList->mid(storedCount, BATCH_STORE_SIZE));
                    storedCount += BATCH_STORE_SIZE;

                    do {
                        bool success;
                        QMap<int, QContactManager::Error> errorMap;
                        if (detailList.isEmpty()) {
                            success = manager()->saveContacts(&batch, &errorMap);
                        } else {
                            success = manager()->saveContacts(&batch, detailList, &errorMap);
                        }
                        if (success) {
                            // We could copy the updated contacts back into saveList here, but it doesn't seem warranted
                            break;
                        }

                        const int errorCount = errorMap.count();
                        if (!errorCount) {
                            break;
                        }

                        // Remove the problematic contacts
                        QList<int> indices = errorMap.keys();
                        QList<int>::const_iterator begin = indices.begin(), it = begin + errorCount;
                        do {
                            int errorIndex = (*--it);
                            const QContact &badContact(batch.at(errorIndex));
                            qCWarning(lcContactsd) << "Failed storing contact" << asString(badContact.id()) << "from:" << location << "error:" << errorMap.value(errorIndex);
                            output(badContact);
                            batch.removeAt(errorIndex);
                        } while (it != begin);
                    } while (true);
                }
                qCDebug(lcContactsd) << "Updated" << saveList->count() << "batched contacts - elapsed:" << t.elapsed() << detailList;
            }
        }
    }

    if (removeList && !removeList->isEmpty()) {
        QElapsedTimer t;
        t.start();

        QList<QContactId>::iterator it = removeList->begin(), end = removeList->end();
        for ( ; it != end; ++it) {
            if (!manager()->removeContact(*it)) {
                qCWarning(lcContactsd) << "Unable to remove contact";
            }
        }
        qCDebug(lcContactsd) << "Removed" << removeList->count() << "individual contacts - elapsed:" << t.elapsed();
    }
}

QList<QContactId> findContactIdsForAccount(const QString &accountPath)
{
    QContactIntersectionFilter filter;
    filter << QContactOriginMetadata::matchGroupId(accountPath);

    QContactCollectionFilter collectionFilter;
    collectionFilter.setCollectionId(telepathyCollectionId(accountPath));
    filter << collectionFilter;

    return manager()->contactIds(filter);
}

QHash<QString, QContact> findExistingContacts(const QStringList &contactAddresses, const QContactCollectionId &collectionId)
{
    QHash<QString, QContact> rv;

    QContactCollectionFilter collectionFilter;
    collectionFilter.setCollectionId(collectionId);

    // If there is a large number of contacts, do a two-step fetch
    const int maxDirectMatches = 10;
    if (contactAddresses.count() > maxDirectMatches) {
        QList<QContactId> ids;
        QSet<QString> addressSet(contactAddresses.toSet());

        // First fetch all telepathy contacts, ID data only
        QContactFetchHint idHint(contactFetchHint(DetailList() << detailType<QContactOriginMetadata>()));
        foreach (const QContact &contact, manager()->contacts(collectionFilter, QList<QContactSortOrder>(), idHint)) {
            const QString &address = stringValue(contact.detail<QContactOriginMetadata>(), QContactOriginMetadata::FieldId);
            if (addressSet.contains(address)) {
                ids.append(contact.id());
            }
        }

        // Now fetch the details of the required contacts by ID
        QContactFetchHint hint(contactFetchHint());
        foreach (const QContact &contact, manager()->contacts(ids, hint)) {
            rv.insert(stringValue(contact.detail<QContactOriginMetadata>(), QContactOriginMetadata::FieldId), contact);
        }
    } else {
        // Just query the ones we need
        QContactIntersectionFilter filter;
        filter << collectionFilter;

        QContactUnionFilter addressFilter;
        foreach (const QString &address, contactAddresses) {
            addressFilter << QContactOriginMetadata::matchId(address);
        }
        filter << addressFilter;

        QContactFetchHint hint(contactFetchHint());
        foreach (const QContact &contact, manager()->contacts(filter, QList<QContactSortOrder>(), hint)) {
            rv.insert(stringValue(contact.detail<QContactOriginMetadata>(), QContactOriginMetadata::FieldId), contact);
        }
    }

    return rv;
}

QHash<QString, QContact> findExistingContacts(const QSet<QString> &contactAddresses, const QContactCollectionId &collectionId)
{
    return findExistingContacts(contactAddresses.toList(), collectionId);
}

QContact findExistingContact(const QString &contactAddress, const QContactCollectionId &collectionId)
{
    QContactIntersectionFilter filter;
    filter << QContactOriginMetadata::matchId(contactAddress);

    QContactCollectionFilter collectionFilter;
    collectionFilter.setCollectionId(collectionId);
    filter << collectionFilter;

    QContactFetchHint hint(contactFetchHint());
    foreach (const QContact &contact, manager()->contacts(filter, QList<QContactSortOrder>(), hint)) {
        // Return the first match we find (there should be only one)
        return contact;
    }

    qCDebug(lcContactsd) << "No matching contact:" << contactAddress;
    return QContact();
}

template<typename T>
T findLinkedDetail(const QContact &owner, const QContactDetail &link)
{
    const QString linkUri(link.detailUri());

    foreach (const T &detail, owner.details<T>()) {
        if (detail.linkedDetailUris().contains(linkUri)) {
            return detail;
        }
    }

    return T();
}

QContactPresence findPresenceForAccount(const QContact &owner, const QContactOnlineAccount &qcoa)
{
    return findLinkedDetail<QContactPresence>(owner, qcoa);
}

QContactAvatar findAvatarForAccount(const QContact &owner, const QContactOnlineAccount &qcoa)
{
    return findLinkedDetail<QContactAvatar>(owner, qcoa);
}

QString imAccount(Tp::AccountPtr account)
{
    return account->objectPath();
}

QString imAccount(CDTpAccount *account)
{
    return imAccount(account->account());
}

QString imAccount(CDTpAccountPtr accountWrapper)
{
    return imAccount(accountWrapper->account());
}

QString imAccount(CDTpContactPtr contactWrapper)
{
    return imAccount(contactWrapper->accountWrapper());
}

QString imAddress(const QString &accountPath, const QString &contactId = QString())
{
    static const QString tmpl = QString::fromLatin1("%1!%2");
    return tmpl.arg(accountPath, contactId.isEmpty() ? QLatin1String("self") : contactId);
}

QString imAddress(Tp::AccountPtr account, const QString &contactId = QString())
{
    return imAddress(imAccount(account), contactId);
}

QString imAddress(CDTpAccountPtr accountWrapper, const QString &contactId = QString())
{
    return imAddress(accountWrapper->account(), contactId);
}

QString imAddress(CDTpContactPtr contactWrapper)
{
    return imAddress(contactWrapper->accountWrapper(), contactWrapper->contact()->id());
}

QString imPresence(const QString &accountPath, const QString &contactId = QString())
{
    static const QString tmpl = QString::fromLatin1("%1!%2!presence");
    return tmpl.arg(accountPath, contactId.isEmpty() ? QLatin1String("self") : contactId);
}

QString imPresence(Tp::AccountPtr account, const QString &contactId = QString())
{
    return imPresence(imAccount(account), contactId);
}

QContactPresence::PresenceState qContactPresenceState(Tp::ConnectionPresenceType presenceType)
{
    switch (presenceType) {
    case Tp::ConnectionPresenceTypeOffline:
        return QContactPresence::PresenceOffline;

    case Tp::ConnectionPresenceTypeAvailable:
        return QContactPresence::PresenceAvailable;

    case Tp::ConnectionPresenceTypeAway:
        return QContactPresence::PresenceAway;

    case Tp::ConnectionPresenceTypeExtendedAway:
        return QContactPresence::PresenceExtendedAway;

    case Tp::ConnectionPresenceTypeHidden:
        return QContactPresence::PresenceHidden;

    case Tp::ConnectionPresenceTypeBusy:
        return QContactPresence::PresenceBusy;

    case Tp::ConnectionPresenceTypeUnknown:
    case Tp::ConnectionPresenceTypeUnset:
    case Tp::ConnectionPresenceTypeError:
        break;

    default:
        qCWarning(lcContactsd) << "Unknown telepathy presence status" << presenceType;
        break;
    }

    return QContactPresence::PresenceUnknown;
}

bool isOnlinePresence(Tp::ConnectionPresenceType presenceType, Tp::AccountPtr account)
{
    switch (presenceType) {
    // Why??
    case Tp::ConnectionPresenceTypeOffline:
        return account->protocolName() == QLatin1String("skype");

    case Tp::ConnectionPresenceTypeUnset:
    case Tp::ConnectionPresenceTypeUnknown:
    case Tp::ConnectionPresenceTypeError:
        return false;

    default:
        break;
    }

    return true;
}

QStringList currentCapabilites(const Tp::CapabilitiesBase &capabilities, Tp::ConnectionPresenceType presenceType, Tp::AccountPtr account)
{
    QStringList current;

    if (capabilities.textChats()) {
        current << asString(CDTpContact::Info::TextChats);
    }

    if (isOnlinePresence(presenceType, account)) {
        if (capabilities.streamedMediaCalls()) {
            current << asString(CDTpContact::Info::StreamedMediaCalls);
        }
        if (capabilities.streamedMediaAudioCalls()) {
            current << asString(CDTpContact::Info::StreamedMediaAudioCalls);
        }
        if (capabilities.streamedMediaVideoCalls()) {
            current << asString(CDTpContact::Info::StreamedMediaAudioVideoCalls);
        }
        if (capabilities.upgradingStreamedMediaCalls()) {
            current << asString(CDTpContact::Info::UpgradingStreamMediaCalls);
        }
        if (capabilities.fileTransfers()) {
            current << asString(CDTpContact::Info::FileTransfers);
        }
    }

    return current;
}

QString saveAccountAvatar(CDTpAccountPtr accountWrapper)
{
    const Tp::Avatar &avatar = accountWrapper->account()->avatar();

    if (avatar.avatarData.isEmpty()) {
        return QString();
    }

    const QString avatarDirPath(CDTpPlugin::cacheFileName(QString::fromLatin1("avatars/account")));

    QDir storageDir(avatarDirPath);
    if (!storageDir.exists() && !storageDir.mkpath(QString::fromLatin1("."))) {
        qWarning() << "Unable to create contacts avatar storage directory:" << storageDir.path();
        return QString();
    }

    QString filename = QString::fromLatin1(QCryptographicHash::hash(avatar.avatarData, QCryptographicHash::Md5).toHex());
    filename = avatarDirPath + QDir::separator() + filename + QString::fromLatin1(".jpg");

    QFile avatarFile(filename);
    if (!avatarFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcContactsd) << "Unable to save account avatar: error opening avatar file" << filename << "for writing";
        return QString();
    }

    avatarFile.write(avatar.avatarData);
    avatarFile.close();

    return filename;
}

void updateFacebookAvatar(QNetworkAccessManager &network, CDTpContactPtr contactWrapper, const QString &facebookId, const QString &avatarType)
{
    const QUrl avatarUrl(QLatin1String("http://graph.facebook.com/v2.6/") % facebookId %
                         QLatin1String("/picture?type=") % avatarType);

    // Initiate an avatar-update operation
    CDTpAvatarUpdate::updateContact(contactWrapper.data(),
                                    network.get(QNetworkRequest(avatarUrl)),
                                    QString::fromLatin1("%1-picture.jpg").arg(facebookId),
                                    avatarType);
}

void updateSocialAvatars(QNetworkAccessManager &network, CDTpContactPtr contactWrapper)
{
    if (network.networkAccessible() == QNetworkAccessManager::NotAccessible) {
        return;
    }

    QRegExp facebookIdPattern(QLatin1String("-(\\d+)@chat\\.facebook\\.com"));

    if (not facebookIdPattern.exactMatch(contactWrapper->contact()->id())) {
        return; // only supporting Facebook avatars right now
    }

    const QString socialId = facebookIdPattern.cap(1);

    // Ignore the square avatar, we only need the large one
    updateFacebookAvatar(network, contactWrapper, socialId, CDTpAvatarUpdate::Large);
}

bool onlineAccountEnabled(const QContactOnlineAccount &qcoa)
{
    return (qcoa.value(QContactOnlineAccount__FieldEnabled).toString() == asString(true));
}

void reportAccountPresence(CDTpDevicePresence *devicePresence, const QContactOnlineAccount &qcoa, const QContactPresence &presence)
{
    emit devicePresence->update(qcoa.value<QString>(QContactOnlineAccount__FieldAccountPath),
                                qcoa.detailUri(),
                                qcoa.serviceProvider(),
                                qcoa.value<QString>(QContactOnlineAccount__FieldServiceProviderDisplayName),
                                qcoa.value<QString>(QContactOnlineAccount__FieldAccountDisplayName),
                                qcoa.value<QString>(QContactOnlineAccount__FieldAccountIconPath),
                                presence.presenceState(),
                                presence.customMessage(),
                                qcoa.value<QString>(QContactOnlineAccount__FieldEnabled) == asString(true));
}

CDTpContact::Changes updateAccountDetails(CDTpDevicePresence *devicePresence, QContact &self, QContactOnlineAccount &qcoa, QContactPresence &presence, CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes)
{
    CDTpContact::Changes selfChanges = 0;

    const QString accountPath(imAccount(accountWrapper));
    qCDebug(lcContactsd) << SRC_LOC << "Update account" << accountPath;

    Tp::AccountPtr account = accountWrapper->account();

    if (changes & CDTpAccount::Presence) {
        Tp::Presence tpPresence(account->currentPresence());

        QContactPresence::PresenceState newState(qContactPresenceState(tpPresence.type()));
        const QString newMessage(tpPresence.statusMessage());

        if ((presence.presenceState() != newState) || (presence.customMessage() != newMessage)) {
            presence.setPresenceState(newState);
            presence.setCustomMessage(newMessage);
            presence.setTimestamp(QDateTime::currentDateTime());

            selfChanges |= CDTpContact::Presence;
        }
    }
    if (changes & CDTpAccount::Nickname) {
        const QString nickname(account->nickname());

        if (presence.nickname() != nickname) {
            presence.setNickname(nickname);
            selfChanges |= CDTpContact::Alias;
        }
    }
    if (changes & CDTpAccount::DisplayName) {
        const QString displayName(account->displayName());

        if (qcoa.value(QContactOnlineAccount__FieldAccountDisplayName) != displayName) {
            qcoa.setValue(QContactOnlineAccount__FieldAccountDisplayName, displayName);
            selfChanges |= CDTpContact::Capabilities;
        }
    }
    if (changes & CDTpAccount::StorageInfo) {
        const QString providerDisplayName(accountWrapper->storageInfo().value(QLatin1String("providerDisplayName")).toString());

        if (qcoa.value(QContactOnlineAccount__FieldServiceProviderDisplayName) != providerDisplayName) {
            qcoa.setValue(QContactOnlineAccount__FieldServiceProviderDisplayName, providerDisplayName);
            selfChanges |= CDTpContact::Capabilities;
        }
    }
    if (changes & CDTpAccount::Avatar) {
        const QString avatarPath(saveAccountAvatar(accountWrapper));

        QContactAvatar avatar(findAvatarForAccount(self, qcoa));

        if (avatarPath.isEmpty()) {
            if (!avatar.isEmpty()) {
                if (!self.removeDetail(&avatar)) {
                    qCWarning(lcContactsd) << SRC_LOC << "Unable to remove avatar for account:" << accountPath;
                }

                selfChanges |= CDTpContact::Avatar;
            }
        } else {
            QUrl avatarUrl(QUrl::fromLocalFile(avatarPath));
            if (avatarUrl != avatar.imageUrl()) {
                avatar.setImageUrl(avatarUrl);
                avatar.setLinkedDetailUris(qcoa.detailUri());

                if (!storeContactDetail(self, avatar, SRC_LOC)) {
                    qCWarning(lcContactsd) << SRC_LOC << "Unable to save avatar for account:" << accountPath;
                }

                selfChanges |= CDTpContact::Avatar;
            }
        }
    }

    // Ensure this account's enabled status is reflected
    if (account->isEnabled() != onlineAccountEnabled(qcoa)) {
        qcoa.setValue(QContactOnlineAccount__FieldEnabled, asString(account->isEnabled()));
        selfChanges |= CDTpContact::Capabilities;
    }

    // If this account is not enabled, ensure the presence indicates unknown state
    if (!account->isEnabled()) {
        if (presence.presenceState() != QContactPresence::PresenceUnknown) {
            presence.setPresenceState(QContactPresence::PresenceUnknown);
            presence.setTimestamp(QDateTime::currentDateTime());
            selfChanges |= CDTpContact::Presence;
        }
    }

    if (selfChanges & CDTpContact::Presence) {
        if (!storeContactDetail(self, presence, SRC_LOC)) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to save presence for self account:" << accountPath;
        }

        // Presence changes also imply potential capabilities changes
        selfChanges |= CDTpContact::Capabilities;
    }

    if (selfChanges & CDTpContact::Capabilities) {
        // The account has changed
        if (!storeContactDetail(self, qcoa, SRC_LOC)) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to save details for self account:" << accountPath;
        }

        // Report the current presence state of this account
        reportAccountPresence(devicePresence, qcoa, presence);
    }

    return selfChanges;
}

template<typename DetailType>
void deleteContactDetails(QContact &existing)
{
    foreach (DetailType detail, existing.details<DetailType>()) {
        if (!existing.removeDetail(&detail)) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to remove obsolete detail:" << detail.detailUri();
        }
    }
}

typedef QHash<QString, int> Dictionary;

Dictionary initPhoneTypes()
{
    Dictionary types;

    types.insert(QLatin1String("bbsl"), QContactPhoneNumber::SubTypeBulletinBoardSystem);
    types.insert(QLatin1String("car"), QContactPhoneNumber::SubTypeCar);
    types.insert(QLatin1String("cell"), QContactPhoneNumber::SubTypeMobile);
    types.insert(QLatin1String("fax"), QContactPhoneNumber::SubTypeFax);
    types.insert(QLatin1String("modem"), QContactPhoneNumber::SubTypeModem);
    types.insert(QLatin1String("pager"), QContactPhoneNumber::SubTypePager);
    types.insert(QLatin1String("video"), QContactPhoneNumber::SubTypeVideo);
    types.insert(QLatin1String("voice"), QContactPhoneNumber::SubTypeVoice);
    // Not sure about these types:
    types.insert(QLatin1String("isdn"), QContactPhoneNumber::SubTypeLandline);
    types.insert(QLatin1String("pcs"), QContactPhoneNumber::SubTypeLandline);

    return types;
}

const Dictionary &phoneTypes()
{
    static Dictionary types(initPhoneTypes());
    return types;
}

Dictionary initAddressTypes()
{
    Dictionary types;

    types.insert(QLatin1String("dom"), QContactAddress::SubTypeDomestic);
    types.insert(QLatin1String("intl"), QContactAddress::SubTypeInternational);
    types.insert(QLatin1String("parcel"), QContactAddress::SubTypeParcel);
    types.insert(QLatin1String("postal"), QContactAddress::SubTypePostal);

    return types;
}

const Dictionary &addressTypes()
{
    static Dictionary types(initAddressTypes());
    return types;
}

Dictionary initGenderTypes()
{
    Dictionary types;

    types.insert(QLatin1String("f"), QContactGender::GenderFemale);
    types.insert(QLatin1String("female"), QContactGender::GenderFemale);
    types.insert(QLatin1String("m"), QContactGender::GenderMale);
    types.insert(QLatin1String("male"), QContactGender::GenderMale);

    return types;
}

const Dictionary &genderTypes()
{
    static Dictionary types(initGenderTypes());
    return types;
}

Dictionary initProtocolTypes()
{
    Dictionary types;

    types.insert(QLatin1String("aim"), QContactOnlineAccount::ProtocolAim);
    types.insert(QLatin1String("icq"), QContactOnlineAccount::ProtocolIcq);
    types.insert(QLatin1String("irc"), QContactOnlineAccount::ProtocolIrc);
    types.insert(QLatin1String("jabber"), QContactOnlineAccount::ProtocolJabber);
    types.insert(QLatin1String("msn"), QContactOnlineAccount::ProtocolMsn);
    types.insert(QLatin1String("qq"), QContactOnlineAccount::ProtocolQq);
    types.insert(QLatin1String("skype"), QContactOnlineAccount::ProtocolSkype);
    types.insert(QLatin1String("yahoo"), QContactOnlineAccount::ProtocolYahoo);

    return types;
}

QContactOnlineAccount::Protocol protocolType(const QString &protocol)
{
    static Dictionary types(initProtocolTypes());

    Dictionary::const_iterator it = types.find(protocol.toLower());
    if (it != types.constEnd()) {
        return static_cast<QContactOnlineAccount::Protocol>(*it);
    }

    return QContactOnlineAccount::ProtocolUnknown;
}

template<typename F1, typename F2>
void replaceNameDetail(F1 getter, F2 setter, QContactName *nameDetail, const QString &value)
{
    if (!value.isEmpty()) {
        (nameDetail->*setter)(value);
    } else {
        // If there is an existing value, remove it
        QString existing((nameDetail->*getter)());
        if (!existing.isEmpty()) {
            (nameDetail->*setter)(value);
        }
    }
}

template<typename T, typename F>
bool detailListsDiffer(const QList<T> &lhs, const QList<T> &rhs, F detailsDiffer)
{
    if (lhs.count() != rhs.count())
        return true;

    typename QList<T>::const_iterator lit = lhs.constBegin(), lend = lhs.constEnd();
    for (typename QList<T>::const_iterator rit = rhs.constBegin(); lit != lend; ++lit, ++rit) {
        if (detailsDiffer(*lit, *rit)) {
            return true;
        }
    }

    return false;
}

// Test detail for validity - for most details, we need some content, beside contexts, etc.
bool invalidDetail(const QContactDetail &detail) { return detail.isEmpty(); }
bool invalidDetail(const QContactAddress &address)
{
    return address.postOfficeBox().isEmpty() &&
           address.street().isEmpty() &&
           address.locality().isEmpty() &&
           address.region().isEmpty() &&
           address.postcode().isEmpty() &&
           address.country().isEmpty();
}
bool invalidDetail(const QContactBirthday &birthday)
{
    return !birthday.date().isValid();
}
bool invalidDetail(const QContactEmailAddress &address)
{
    return address.emailAddress().isEmpty();
}
bool invalidDetail(const QContactName &name)
{
    return name.prefix().isEmpty() &&
           name.firstName().isEmpty() &&
           name.middleName().isEmpty() &&
           name.lastName().isEmpty() &&
           name.suffix().isEmpty();
}
bool invalidDetail(const QContactNote &note)
{
    return note.note().isEmpty();
}
bool invalidDetail(const QContactOrganization &org)
{
    return org.title().isEmpty() &&
           org.role().isEmpty() &&
           org.name().isEmpty() &&
           org.department().isEmpty();
}
bool invalidDetail(const QContactPhoneNumber &phoneNumber)
{
    return phoneNumber.number().isEmpty();
}
bool invalidDetail(const QContactUrl &url)
{
    return url.url().isEmpty();
}

template<typename T>
bool replaceDetails(QContact &contact, QList<T> &details, const QString &address, const QString &location)
{
    deleteContactDetails<T>(contact);

    foreach (T detail, details) {
        if (!invalidDetail(detail)) {
            if (!storeContactDetail(contact, detail, location)) {
                qCWarning(lcContactsd) << SRC_LOC << "Unable to save detail to contact:" << address;
            }
        }
    }

    return true;
}

template<typename T>
bool replaceDetails(QContact &contact, T &detail, const QString &address, const QString &location)
{
    return replaceDetails(contact, QList<T>() << detail, address, location);
}

CDTpContact::Changes updateContactDetails(QNetworkAccessManager &network, QContact &existing, CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    const QString contactAddress(imAddress(contactWrapper));
    qCDebug(lcContactsd) << "Update contact" << contactAddress;

    Tp::ContactPtr contact = contactWrapper->contact();

    CDTpContact::Changes contactChanges;

    // Apply changes
    if (changes & CDTpContact::Alias) {
        QContactNickname nickname = existing.detail<QContactNickname>();

        const QString newNickname(contact->alias().trimmed());
        if (nickname.nickname() != newNickname) {
            nickname.setNickname(newNickname);

            if (!storeContactDetail(existing, nickname, SRC_LOC)) {
                qCWarning(lcContactsd) << SRC_LOC << "Unable to save alias to contact for:" << contactAddress;
            }

            contactChanges |= CDTpContact::Alias;
        }

        // The alias is also reflected in the presence
        changes |= CDTpContact::Presence;
    }
    if (changes & CDTpContact::Presence) {
        Tp::Presence tpPresence(contact->presence());

        QContactPresence presence = existing.detail<QContactPresence>();

        const QContactPresence::PresenceState newState(qContactPresenceState(tpPresence.type()));
        const QString newMessage(tpPresence.statusMessage());
        const QString newNickname(contact->alias().trimmed());

        if (presence.presenceState() != newState || presence.customMessage() != newMessage || presence.nickname() != newNickname) {
            presence.setPresenceState(newState);
            presence.setCustomMessage(newMessage);
            presence.setNickname(newNickname);
            presence.setTimestamp(QDateTime::currentDateTime());

            if (!storeContactDetail(existing, presence, SRC_LOC)) {
                qCWarning(lcContactsd) << SRC_LOC << "Unable to save presence to contact for:" << contactAddress;
            }

            contactChanges |= CDTpContact::Presence;
        }

        // Since we use static account capabilities as fallback, each presence also implies
        // a capability change. This doesn't fit the pure school of Telepathy, but we really
        // should not drop the static caps fallback at this stage.
        changes |= CDTpContact::Capabilities;
    }
    if (changes & CDTpContact::Capabilities) {
        QContactOnlineAccount qcoa = existing.detail<QContactOnlineAccount>();
        CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();
        Tp::AccountPtr account = accountWrapper->account();

        const QString providerDisplayName(accountWrapper->storageInfo().value(QLatin1String("providerDisplayName")).toString());
        const QStringList newCapabilities(currentCapabilites(contact->capabilities(), contact->presence().type(), account));

        if (qcoa.capabilities() != newCapabilities ||
            onlineAccountEnabled(qcoa) != account->isEnabled() ||
            qcoa.value(QContactOnlineAccount__FieldAccountDisplayName) != account->displayName() ||
            qcoa.value(QContactOnlineAccount__FieldServiceProviderDisplayName) != providerDisplayName)
        {
            qcoa.setCapabilities(newCapabilities);
            qcoa.setValue(QContactOnlineAccount__FieldEnabled, asString(account->isEnabled()));
            qcoa.setValue(QContactOnlineAccount__FieldAccountDisplayName, account->displayName());
            qcoa.setValue(QContactOnlineAccount__FieldServiceProviderDisplayName, providerDisplayName);

            if (!storeContactDetail(existing, qcoa, SRC_LOC)) {
                qCWarning(lcContactsd) << SRC_LOC << "Unable to save capabilities to contact for:" << contactAddress;
            }

            contactChanges |= CDTpContact::Capabilities;
        }
    }
    if (changes & CDTpContact::Information) {
        if (contactWrapper->isInformationKnown()) {
            // Extract the current information state from the info fields
            QList<QContactAddress> newAddresses;
            QContactBirthday newBirthday;
            QList<QContactEmailAddress> newEmailAddresses;
            QContactGender newGender;
            QContactName newName;
            QList<QContactNickname> newNicknames;
            QList<QContactNote> newNotes;
            QList<QContactOrganization> newOrganizations;
            QList<QContactPhoneNumber> newPhoneNumbers;
            QList<QContactUrl> newUrls;

            Tp::ContactInfoFieldList listContactInfo = contact->infoFields().allFields();
            if (listContactInfo.count() != 0) {
                const int defaultContext(QContactDetail::ContextOther);
                const int homeContext(QContactDetail::ContextHome);
                const int workContext(QContactDetail::ContextWork);

                QContactOrganization organizationDetail;
                QContactName nameDetail;
                QString formattedName;
                bool structuredName = false;

                // Add any information reported by telepathy
                foreach (const Tp::ContactInfoField &field, listContactInfo) {
                    if (field.fieldValue.count() == 0) {
                        continue;
                    }

                    // Extract field types
                    QStringList subTypes;
                    int detailContext = -1;
                    const int invalidContext = -1;

                    foreach (const QString &param, field.parameters) {
                        if (!param.startsWith(QLatin1String("type="))) {
                            continue;
                        }
                        const QString type = param.mid(5);
                        if (type == QLatin1String("home")) {
                            detailContext = homeContext;
                        } else if (type == QLatin1String("work")) {
                            detailContext = workContext;
                        } else if (!subTypes.contains(type)){
                            subTypes << type;
                        }
                    }

                    if (field.fieldName == QLatin1String("tel")) {
                        QList<int> selectedTypes;
                        foreach (const QString &type, subTypes) {
                            Dictionary::const_iterator it = phoneTypes().find(type.toLower());
                            if (it != phoneTypes().constEnd()) {
                                selectedTypes.append(*it);
                            }
                        }
                        if (selectedTypes.isEmpty()) {
                            // Assume landline
                            selectedTypes.append(QContactPhoneNumber::SubTypeLandline);
                        }

                        QContactPhoneNumber phoneNumberDetail;
                        phoneNumberDetail.setContexts(detailContext == invalidContext ? defaultContext : detailContext);
                        phoneNumberDetail.setNumber(asString(field, 0).trimmed());
                        phoneNumberDetail.setSubTypes(selectedTypes);

                        newPhoneNumbers.append(phoneNumberDetail);
                    } else if (field.fieldName == QLatin1String("adr")) {
                        QList<int> selectedTypes;
                        foreach (const QString &type, subTypes) {
                            Dictionary::const_iterator it = addressTypes().find(type.toLower());
                            if (it != addressTypes().constEnd()) {
                                selectedTypes.append(*it);
                            }
                        }

                        // QContactAddress does not support extended street address, so combine the fields
                        QStringList streetParts;
                        for (int i = 1; i <= 2; ++i) {
                            QString part(asString(field, i).trimmed());
                            if (!part.isEmpty()) {
                                streetParts.append(part);
                            }
                        }

                        QContactAddress addressDetail;
                        if (detailContext != invalidContext) {
                            addressDetail.setContexts(detailContext);
                        }
                        if (selectedTypes.isEmpty()) {
                            addressDetail.setSubTypes(selectedTypes);
                        }
                        addressDetail.setPostOfficeBox(asString(field, 0).trimmed());
                        addressDetail.setStreet(streetParts.join(QString::fromLatin1("\n")));
                        addressDetail.setLocality(asString(field, 3).trimmed());
                        addressDetail.setRegion(asString(field, 4).trimmed());
                        addressDetail.setPostcode(asString(field, 5).trimmed());
                        addressDetail.setCountry(asString(field, 6).trimmed());

                        newAddresses.append(addressDetail);
                    } else if (field.fieldName == QLatin1String("email")) {
                        QContactEmailAddress emailDetail;
                        if (detailContext != invalidContext) {
                            emailDetail.setContexts(detailContext);
                        }
                        emailDetail.setEmailAddress(asString(field, 0).trimmed());

                        newEmailAddresses.append(emailDetail);
                    } else if (field.fieldName == QLatin1String("url")) {
                        QContactUrl urlDetail;
                        if (detailContext != invalidContext) {
                            urlDetail.setContexts(detailContext);
                        }
                        urlDetail.setUrl(asString(field, 0).trimmed());

                        newUrls.append(urlDetail);
                    } else if (field.fieldName == QLatin1String("title")) {
                        organizationDetail.setTitle(asString(field, 0).trimmed());
                        if (detailContext != invalidContext) {
                            organizationDetail.setContexts(detailContext);
                        }
                    } else if (field.fieldName == QLatin1String("role")) {
                        organizationDetail.setRole(asString(field, 0).trimmed());
                        if (detailContext != invalidContext) {
                            organizationDetail.setContexts(detailContext);
                        }
                    } else if (field.fieldName == QLatin1String("org")) {
                        organizationDetail.setName(asString(field, 0).trimmed());
                        organizationDetail.setDepartment(asStringList(field, 1));
                        if (detailContext != invalidContext) {
                            organizationDetail.setContexts(detailContext);
                        }

                        newOrganizations.append(organizationDetail);

                        // Clear out the stored details
                        organizationDetail = QContactOrganization();
                    } else if (field.fieldName == QLatin1String("n")) {
                        if (detailContext != invalidContext) {
                            nameDetail.setContexts(detailContext);
                        }

                        replaceNameDetail(&QContactName::lastName, &QContactName::setLastName, &nameDetail, asString(field, 0).trimmed());
                        replaceNameDetail(&QContactName::firstName, &QContactName::setFirstName, &nameDetail, asString(field, 1).trimmed());
                        replaceNameDetail(&QContactName::middleName, &QContactName::setMiddleName, &nameDetail, asString(field, 2).trimmed());
                        replaceNameDetail(&QContactName::prefix, &QContactName::setPrefix, &nameDetail, asString(field, 3).trimmed());
                        replaceNameDetail(&QContactName::suffix, &QContactName::setSuffix, &nameDetail, asString(field, 4).trimmed());

                        structuredName = true;
                    } else if (field.fieldName == QLatin1String("fn")) {
                        const QString fn(asString(field, 0).trimmed());
                        if (!fn.isEmpty()) {
                            if (detailContext != invalidContext) {
                                nameDetail.setContexts(detailContext);
                            }
                            formattedName = fn;
                        }
                    } else if (field.fieldName == QLatin1String("nickname")) {
                        const QString nickname(asString(field, 0).trimmed());
                        if (!nickname.isEmpty()) {
                            QContactNickname nicknameDetail;
                            nicknameDetail.setNickname(nickname);
                            if (detailContext != invalidContext) {
                                nicknameDetail.setContexts(detailContext);
                            }

                            newNicknames.append(nicknameDetail);

                            // Use the nickname as the customLabel if we have no 'fn' data
                            if (formattedName.isEmpty()) {
                                formattedName = nickname;
                            }
                        }
                    } else if (field.fieldName == QLatin1String("note") ||
                               field.fieldName == QLatin1String("desc")) {
                        QContactNote noteDetail;
                        if (detailContext != invalidContext) {
                            noteDetail.setContexts(detailContext);
                        }
                        noteDetail.setNote(asString(field, 0).trimmed());

                        newNotes.append(noteDetail);
                    } else if (field.fieldName == QLatin1String("bday")) {
                        /* FIXME: support more date format for compatibility */
                        const QString dateText(asString(field, 0));

                        QDate date = QDate::fromString(dateText, QLatin1String("yyyy-MM-dd"));
                        if (!date.isValid()) {
                            date = QDate::fromString(dateText, QLatin1String("yyyyMMdd"));
                        }
                        if (!date.isValid()) {
                            date = QDate::fromString(dateText, Qt::ISODate);
                        }

                        if (date.isValid()) {
                            QContactBirthday birthdayDetail;
                            birthdayDetail.setDate(date);

                            newBirthday = birthdayDetail;
                        } else {
                            qCDebug(lcContactsd) << "Unsupported bday format:" << field.fieldValue[0];
                        }
                    } else if (field.fieldName == QLatin1String("x-gender")) {
                        const QString type(field.fieldValue.at(0));

                        Dictionary::const_iterator it = genderTypes().find(type.toLower());
                        if (it != addressTypes().constEnd()) {
                            QContactGender genderDetail;
                            genderDetail.setGender(static_cast<QContactGender::GenderType>(*it));

                            newGender = genderDetail;
                        } else {
                            qCDebug(lcContactsd) << "Unsupported gender type:" << type;
                        }
                    } else {
                        qCDebug(lcContactsd) << "Unsupported contact info field" << field.fieldName;
                    }
                }

                if (structuredName || !formattedName.isEmpty()) {
                    if (!structuredName) {
                        SeasideCache::decomposeDisplayLabel(formattedName, &nameDetail);
                    }

                    if (!formattedName.isEmpty()) {
                        nameDetail.setValue(QContactName::FieldCustomLabel, formattedName);
                    }

                    newName = nameDetail;
                }
            }

            // For all detail types, test if there has been any change
            bool changed = false;

            const QList<QContactAddress> oldAddresses = existing.details<QContactAddress>();
            if (detailListsDiffer(oldAddresses, newAddresses,
                [](const QContactAddress &oldAddress, const QContactAddress &newAddress) {
                    if ((oldAddress.contexts() != newAddress.contexts()) ||
                        (oldAddress.subTypes() != newAddress.subTypes()) ||
                        (oldAddress.postOfficeBox() != newAddress.postOfficeBox()) ||
                        (oldAddress.street() != newAddress.street()) ||
                        (oldAddress.locality() != newAddress.locality()) ||
                        (oldAddress.region() != newAddress.region()) ||
                        (oldAddress.postcode() != newAddress.postcode()) ||
                        (oldAddress.country() != newAddress.country())) {
                        return true;
                    }
                    return false;
                })
            ) {
                changed |= replaceDetails(existing, newAddresses, contactAddress, SRC_LOC);
            }

            QContactBirthday oldBirthday = existing.detail<QContactBirthday>();
            if (!oldBirthday.isEmpty() && newBirthday.isEmpty()) {
                deleteContactDetails<QContactBirthday>(existing);
            } else if ((oldBirthday.isEmpty() && !newBirthday.isEmpty()) ||
                       (oldBirthday.date() != newBirthday.date())) {
                changed |= replaceDetails(existing, newBirthday, contactAddress, SRC_LOC);
            }

            const QList<QContactEmailAddress> oldEmailAddresses = existing.details<QContactEmailAddress>();
            if (detailListsDiffer(oldEmailAddresses, newEmailAddresses,
                [](const QContactEmailAddress &oldEmailAddress, const QContactEmailAddress &newEmailAddress) {
                    if ((oldEmailAddress.contexts() != newEmailAddress.contexts()) ||
                        (oldEmailAddress.emailAddress() != newEmailAddress.emailAddress())) {
                        return true;
                    }
                    return false;
                })
            ) {
                changed |= replaceDetails(existing, newEmailAddresses, contactAddress, SRC_LOC);
            }

            QContactGender oldGender = existing.detail<QContactGender>();
            if (!oldGender.isEmpty() && newGender.isEmpty()) {
                deleteContactDetails<QContactGender>(existing);
            } else if ((oldGender.isEmpty() && !newGender.isEmpty()) ||
                       (oldGender.gender() != newGender.gender())) {
                changed |= replaceDetails(existing, newGender, contactAddress, SRC_LOC);
            }

            QContactName oldName = existing.detail<QContactName>();
            if ((oldName.firstName() != newName.firstName()) ||
                (oldName.middleName() != newName.middleName()) ||
                (oldName.lastName() != newName.lastName()) ||
                (oldName.value<QString>(QContactName::FieldCustomLabel) != newName.value<QString>(QContactName::FieldCustomLabel)) ||
                (oldName.prefix() != newName.prefix()) ||
                (oldName.suffix() != newName.suffix())) {
                changed |= replaceDetails(existing, newName, contactAddress, SRC_LOC);
            }

            // Nicknames are different to other list types, since they can come from the presence info as well
            const QList<QContactNickname> oldNicknames = existing.details<QContactNickname>();
            foreach (QContactNickname newNickname, newNicknames) {
                bool found = false;
                foreach (const QContactNickname &oldNickname, oldNicknames) {
                    if ((oldNickname.contexts() == newNickname.contexts()) &&
                        (oldNickname.nickname() == newNickname.nickname())) {
                        // Nickname already present
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // Add this nickname
                    if (!storeContactDetail(existing, newNickname, SRC_LOC)) {
                        qCWarning(lcContactsd) << SRC_LOC << "Unable to save nickname to contact for:" << contactAddress;
                    }
                    changed = true;
                }
            }

            const QList<QContactNote> oldNotes = existing.details<QContactNote>();
            if (detailListsDiffer(oldNotes, newNotes,
                [](const QContactNote &oldNote, const QContactNote &newNote) {
                    if ((oldNote.contexts() != newNote.contexts()) ||
                        (oldNote.note() != newNote.note())) {
                        return true;
                    }
                    return false;
                })
            ) {
                changed |= replaceDetails(existing, newNotes, contactAddress, SRC_LOC);
            }

            const QList<QContactOrganization> oldOrganizations = existing.details<QContactOrganization>();
            if (detailListsDiffer(oldOrganizations, newOrganizations,
                [](const QContactOrganization &oldOrganization, const QContactOrganization &newOrganization) {
                    if ((oldOrganization.contexts() != newOrganization.contexts()) ||
                        (oldOrganization.name() != newOrganization.name()) ||
                        (oldOrganization.department() != newOrganization.department())) {
                        return true;
                    }
                    return false;
                })
            ) {
                changed |= replaceDetails(existing, newOrganizations, contactAddress, SRC_LOC);
            }

            const QList<QContactPhoneNumber> oldPhoneNumbers = existing.details<QContactPhoneNumber>();
            if (detailListsDiffer(oldPhoneNumbers, newPhoneNumbers,
                [](const QContactPhoneNumber &oldPhoneNumber, const QContactPhoneNumber &newPhoneNumber) {
                    if ((oldPhoneNumber.contexts() != newPhoneNumber.contexts()) ||
                        (oldPhoneNumber.subTypes() != newPhoneNumber.subTypes()) ||
                        (oldPhoneNumber.number() != newPhoneNumber.number())) {
                        return true;
                    }
                    return false;
                })
            ) {
                changed |= replaceDetails(existing, newPhoneNumbers, contactAddress, SRC_LOC);
            }

            const QList<QContactUrl> oldUrls = existing.details<QContactUrl>();
            if (detailListsDiffer(oldUrls, newUrls,
                [](const QContactUrl &oldUrl, const QContactUrl &newUrl) {
                    if ((oldUrl.contexts() != newUrl.contexts()) ||
                        (oldUrl.url() != newUrl.url())) {
                        return true;
                    }
                    return false;
                })
            ) {
                changed |= replaceDetails(existing, newUrls, contactAddress, SRC_LOC);
            }

            if (changed) {
                contactChanges |= CDTpContact::Information;
            }
        }
    }
    if (changes & CDTpContact::Avatar) {
        // Prefer the large avatar if available
        QString avatarPath(contactWrapper->largeAvatarPath());
        if (avatarPath.isEmpty()) {
            avatarPath = contact->avatarData().fileName;
        }

        QContactAvatar avatar = existing.detail<QContactAvatar>();

        if (avatarPath.isEmpty()) {
            if (!avatar.isEmpty()) {
                // Remove the avatar detail
                if (!existing.removeDetail(&avatar)) {
                    qCWarning(lcContactsd) << SRC_LOC << "Unable to remove avatar from contact:" << contactAddress;
                }

                contactChanges |= CDTpContact::Avatar;
            }
        } else {
            QUrl avatarUrl(QUrl::fromLocalFile(avatarPath));
            if (avatarUrl != avatar.imageUrl()) {
                avatar.setImageUrl(avatarUrl);

                QContactOnlineAccount qcoa = existing.detail<QContactOnlineAccount>();
                avatar.setLinkedDetailUris(qcoa.detailUri());

                if (!storeContactDetail(existing, avatar, SRC_LOC)) {
                    qCWarning(lcContactsd) << SRC_LOC << "Unable to save avatar for contact:" << contactAddress;
                }

                contactChanges |= CDTpContact::Avatar;
            }
        }
    }
    if (changes & CDTpContact::DefaultAvatar) {
        updateSocialAvatars(network, contactWrapper);
    }
    /* What is this about?
    if (changes & CDTpContact::Authorization) {
        qCDebug(lcContactsd) << "  authorization changed";
        g.addPattern(imAddress, nco::imAddressAuthStatusFrom::resource(),
                presenceState(contact->subscriptionState()));
        g.addPattern(imAddress, nco::imAddressAuthStatusTo::resource(),
                presenceState(contact->publishState()));
    }
    */

    return contactChanges;
}

template<typename T, typename R>
QList<R> forEachItem(const QList<T> &list, R (*f)(const T&))
{
    QList<R> rv;
    rv.reserve(list.count());

    foreach (const T &item, list) {
        const R& r = f(item);
        rv.append(r);
    }

    return rv;
}

QString extractAccountPath(const CDTpAccountPtr &accountWrapper)
{
    return imAccount(accountWrapper);
}

void addIconPath(QContactOnlineAccount &qcoa, Tp::AccountPtr account)
{
    QString iconName = account->iconName().trimmed();

    // Ignore any default value returned by telepathy
    if (!iconName.startsWith(QLatin1String("im-"))) {
        qcoa.setValue(QContactOnlineAccount__FieldAccountIconPath, iconName);
    }
}

} // namespace


CDTpStorage::CDTpStorage(QObject *parent)
    : QObject(parent)
    , mDevicePresence(new CDTpDevicePresence)
    , mDisplayLabelOrder(FirstNameFirst)
    , mDisplayLabelOrderConf(QStringLiteral("/org/nemomobile/contacts/display_label_order"))
{
    connect(mDevicePresence, SIGNAL(requestUpdate()), this, SLOT(reportPresenceStates()));

    connect(&mDisplayLabelOrderConf, SIGNAL(valueChanged()), this, SLOT(displayLabelOrderChanged()));

    QVariant displayLabelOrder = mDisplayLabelOrderConf.value();
    if (displayLabelOrder.isValid())
        mDisplayLabelOrder = static_cast<DisplayLabelOrder>(displayLabelOrder.toInt());

    mUpdateTimer.setInterval(UPDATE_TIMEOUT);
    mUpdateTimer.setSingleShot(true);
    connect(&mUpdateTimer, SIGNAL(timeout()), SLOT(onUpdateQueueTimeout()));

    mWaitTimer.invalidate();
}

CDTpStorage::~CDTpStorage()
{
}

/* Set generic account properties of a QContactOnlineAccount. Does not set:
 * detailUri
 * linkedDetailUris (i.e. presence)
 * enabled
 * accountUri
 */
static void updateContactAccount(QContactOnlineAccount &qcoa, CDTpAccountPtr accountWrapper)
{
    Tp::AccountPtr account = accountWrapper->account();

    qcoa.setValue(QContactOnlineAccount__FieldAccountPath, imAccount(account));
    qcoa.setProtocol(protocolType(account->protocolName()));
    qcoa.setServiceProvider(account->serviceName());

    QString providerDisplayName = accountWrapper->storageInfo().value(QLatin1String("providerDisplayName")).toString();
    qcoa.setValue(QContactOnlineAccount__FieldServiceProviderDisplayName, providerDisplayName);
    qcoa.setValue(QContactOnlineAccount__FieldAccountDisplayName, account->displayName());

    addIconPath(qcoa, account);
}

void CDTpStorage::addNewAccount()
{
    CDTpAccount *account = qobject_cast<CDTpAccount*>(sender());
    if (!account)
        return;

    // Disconnect the signal
    disconnect(account, SIGNAL(readyChanged()), this, SLOT(addNewAccount()));

    // Create a new contact collection for this account
    const QContactCollectionId collectionId = telepathyCollectionId(imAccount(account));
    QContact self(selfContact(collectionId));

    qCDebug(lcContactsd) << "New account" << imAccount(account) << "is ready, calling delayed addNewAccount";
    addNewAccount(self, CDTpAccountPtr(account));
}

void CDTpStorage::addNewAccount(QContact &self, CDTpAccountPtr accountWrapper)
{
    Tp::AccountPtr account = accountWrapper->account();

    const QString accountPath(imAccount(account));
    const QString accountAddress(imAddress(account));
    const QString accountPresence(imPresence(account));

    if (!accountWrapper->isReady()) {
        qCDebug(lcContactsd) << "Waiting to create new self account" << accountPath << "until ready";
        connect(accountWrapper.data(), SIGNAL(readyChanged()), SLOT(addNewAccount()));
        return;
    }

    qCDebug(lcContactsd) << "Creating new self account - account:" << accountPath << "address:" << accountAddress;

    // Create a new QCOA for this account
    QContactOnlineAccount newAccount;
    updateContactAccount(newAccount, accountWrapper);

    newAccount.setDetailUri(accountAddress);
    newAccount.setLinkedDetailUris(accountPresence);
    newAccount.setValue(QContactOnlineAccount__FieldEnabled, asString(account->isEnabled()));
    newAccount.setAccountUri(account->normalizedName());

    // Add the new account to the self contact
    if (!storeContactDetail(self, newAccount, SRC_LOC)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to add account to self contact for:" << accountPath;
        return;
    }

    // Create a presence detail for this account
    QContactPresence presence;

    presence.setDetailUri(accountPresence);
    presence.setLinkedDetailUris(accountAddress);
    presence.setPresenceState(qContactPresenceState(Tp::ConnectionPresenceTypeUnknown));

    if (!storeContactDetail(self, presence, SRC_LOC)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to add presence to self contact for:" << accountPath;
        return;
    }

    // Store any information from the account
    CDTpContact::Changes selfChanges = updateAccountDetails(mDevicePresence, self, newAccount, presence, accountWrapper, CDTpAccount::All);

    storeSelfContact(mDevicePresence, self, SRC_LOC, selfChanges);
}

void CDTpStorage::removeExistingAccount(QContact &self, QContactOnlineAccount &existing)
{
    Q_UNUSED(self)

    const QString accountPath(stringValue(existing, QContactOnlineAccount__FieldAccountPath));

    qCDebug(lcContactsd) << "Remove account for path" << accountPath
            << " and collection id" << telepathyCollectionId(accountPath);

    // Delete the collection and its contacts.
    QtContactsSqliteExtensions::ContactManagerEngine *cme = QtContactsSqliteExtensions::contactManagerEngine(*manager());
    QContactManager::Error error = QContactManager::NoError;
    if (!cme->storeChanges(nullptr,
                          nullptr,
                          QList<QContactCollectionId>() << telepathyCollectionId(accountPath),
                          QtContactsSqliteExtensions::ContactManagerEngine::PreserveLocalChanges,
                          true,
                          &error)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to remove linked contacts for account:" << accountPath
                  << "error:" << error;
    }
}

bool CDTpStorage::initializeNewContact(QContact &newContact, CDTpAccountPtr accountWrapper, const QString &contactId, const QString &alias)
{
    Tp::AccountPtr account = accountWrapper->account();

    const QString accountPath(imAccount(account));
    const QString contactAddress(imAddress(account, contactId));
    const QString contactPresence(imPresence(account, contactId));

    qCDebug(lcContactsd) << "Creating new contact - address:" << contactAddress;

    // This contact belongs to a telepathy address book
    newContact.setCollectionId(telepathyCollectionId(accountPath));

    // Create a metadata field to link the contact with the telepathy data
    QContactOriginMetadata metadata;
    metadata.setId(contactAddress);
    metadata.setGroupId(imAccount(account));
    metadata.setEnabled(true);
    if (!storeContactDetail(newContact, metadata, SRC_LOC)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to add metadata to contact:" << contactAddress;
        return false;
    }

    // Create a new QCOA for this contact
    QContactOnlineAccount newAccount;
    updateContactAccount(newAccount, accountWrapper);

    newAccount.setDetailUri(contactAddress);
    newAccount.setLinkedDetailUris(contactPresence);
    newAccount.setValue(QContactOnlineAccount__FieldEnabled, asString(true));
    newAccount.setAccountUri(contactId);

    // Add the new account to the contact
    if (!storeContactDetail(newContact, newAccount, SRC_LOC)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to save account to contact for:" << contactAddress;
        return false;
    }

    // Create a presence detail for this contact
    QContactPresence presence;

    presence.setDetailUri(contactPresence);
    presence.setLinkedDetailUris(contactAddress);
    presence.setPresenceState(qContactPresenceState(Tp::ConnectionPresenceTypeUnknown));
    if (!alias.isEmpty()) {
        presence.setNickname(alias);
    }

    if (!storeContactDetail(newContact, presence, SRC_LOC)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to save presence to contact for:" << contactAddress;
        return false;
    }

    // Initially we will have no name detail - try to extract it from the alias
    if (!alias.isEmpty()) {
        QContactName name;

        SeasideCache::decomposeDisplayLabel(alias, &name);

        name.setValue(QContactName::FieldCustomLabel, alias);

        if (!storeContactDetail(newContact, name, SRC_LOC)) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to save name to contact for:" << contactAddress;
            return false;
        }
    }

    return true;
}

bool CDTpStorage::initializeNewContact(QContact &newContact, CDTpContactPtr contactWrapper)
{
    CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();
    Tp::ContactPtr contact = contactWrapper->contact();

    const QString id(contact->id());
    const QString alias(contact->alias().trimmed());

    return initializeNewContact(newContact, accountWrapper, id, alias);
}

void CDTpStorage::updateContactChanges(CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    ContactChangeSet saveSet;
    QList<QContactId> removeList;

    QContact existing = findExistingContact(imAddress(contactWrapper),
                                            telepathyCollectionId(imAccount(contactWrapper)));
    updateContactChanges(contactWrapper, changes, existing, &saveSet, &removeList);

    updateContacts(SRC_LOC, &saveSet, &removeList);
}

void CDTpStorage::updateContactChanges(CDTpContactPtr contactWrapper, CDTpContact::Changes changes, QContact &existing, ContactChangeSet *saveSet, QList<QContactId> *removeList)
{
    const QString accountPath(imAccount(contactWrapper));
    const QString contactAddress(imAddress(contactWrapper));

    if (changes & CDTpContact::Deleted) {
        // This contact has been deleted
        if (!existing.isEmpty()) {
            removeList->append(existing.id());
        }
    } else {
        bool needAllChanges = false;
        if (existing.isEmpty()) {
            if (!initializeNewContact(existing, contactWrapper)) {
                qCWarning(lcContactsd) << SRC_LOC << "Unable to create contact for account:" << accountPath << contactAddress;
                return;
            }
            needAllChanges = true;
        }

        changes = updateContactDetails(mNetwork, existing, contactWrapper, changes);
        if (needAllChanges) changes = CDTpContact::All;
        appendContactChange(saveSet, existing, changes);
    }
}

QList<CDTpContactPtr> accountContacts(CDTpAccountPtr accountWrapper)
{
    QList<CDTpContactPtr> rv;

    QSet<QString> ids;
    foreach (CDTpContactPtr contactWrapper, accountWrapper->contacts()) {
        const QString id(contactWrapper->contact()->id());
        if (ids.contains(id))
            continue;

        ids.insert(id);
        rv.append(contactWrapper);
    }

    return rv;
}

void CDTpStorage::updateAccount()
{
    CDTpAccount *account = qobject_cast<CDTpAccount*>(sender());
    if (!account)
        return;

    // Disconnect the signal
    disconnect(account, SIGNAL(readyChanged()), this, SLOT(updateAccount()));

    const QString accountPath(imAccount(account));

    qCDebug(lcContactsd) << "Delayed update of account" << accountPath << "is ready";
    CDTpAccount::Changes changes = CDTpAccount::All;
    QMap<QString, CDTpAccount::Changes>::iterator it = m_accountPendingChanges.find(accountPath);
    if (it != m_accountPendingChanges.end()) {
        changes = it.value();
        m_accountPendingChanges.erase(it);
    }
    updateAccount(CDTpAccountPtr(account), changes);
}

void CDTpStorage::updateAccountChanges(QContact &self, QContactOnlineAccount &qcoa, CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes)
{
    Tp::AccountPtr account = accountWrapper->account();

    const QString accountPath(imAccount(account));
    const QString accountAddress(imAddress(account));

    if (!accountWrapper->isReady()) {
        qCDebug(lcContactsd) << "Delaying update of account" << accountPath << "address" << accountAddress << "until ready";
        QMap<QString, CDTpAccount::Changes>::iterator it = m_accountPendingChanges.find(accountPath);
        if (it != m_accountPendingChanges.end()) {
            it.value() |= changes;
        } else {
            m_accountPendingChanges.insert(accountPath, changes);
            connect(accountWrapper.data(), SIGNAL(readyChanged()), SLOT(updateAccount()));
        }
        return;
    }

    qCDebug(lcContactsd) << "Synchronizing self account - account:" << accountPath << "address:" << accountAddress;

    QContactPresence presence(findPresenceForAccount(self, qcoa));
    if (presence.isEmpty()) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to find presence to match account:" << accountPath;
    }

    CDTpContact::Changes selfChanges = updateAccountDetails(mDevicePresence, self, qcoa, presence, accountWrapper, changes);

    if (!storeSelfContact(mDevicePresence, self, SRC_LOC, selfChanges)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to save self contact - error:" << manager()->error();
    }

    if (account->isEnabled() && accountWrapper->hasRoster()) {
        QHash<QString, CDTpContact::Changes> allChanges;

        // Update all contacts reported in the roster changes of this account
        const QHash<QString, CDTpContact::Changes> rosterChanges = accountWrapper->rosterChanges();
        QHash<QString, CDTpContact::Changes>::ConstIterator it = rosterChanges.constBegin(),
                                                            end = rosterChanges.constEnd();
        for ( ; it != end; ++it) {
            const QString address = imAddress(accountPath, it.key());
            CDTpContact::Changes flags = it.value() | CDTpContact::Presence;

            // If account display name changes, update QCOA of all contacts
            if (changes & CDTpAccount::DisplayName)
                flags |= CDTpContact::Capabilities;

            // We always update contact presence since this method is called after a presence change
            allChanges.insert(address, flags);
        }

        QList<CDTpContactPtr> tpContacts(accountContacts(accountWrapper));

        QStringList contactAddresses;
        foreach (const CDTpContactPtr &contactWrapper, tpContacts) {
            const QString address = imAddress(accountPath, contactWrapper->contact()->id());
            contactAddresses.append(address);
        }

        // Retrieve the existing contacts in a single batch
        QHash<QString, QContact> existingContacts = findExistingContacts(
                    contactAddresses, telepathyCollectionId(imAccount(accountWrapper)));

        ContactChangeSet saveSet;
        QList<QContactId> removeList;

        foreach (const CDTpContactPtr &contactWrapper, tpContacts) {
            const QString address = imAddress(accountPath, contactWrapper->contact()->id());

            QHash<QString, CDTpContact::Changes>::Iterator cit = allChanges.find(address);
            if (cit == allChanges.end()) {
                qCWarning(lcContactsd) << SRC_LOC << "No changes found for contact:" << address;
                continue;
            }

            CDTpContact::Changes changes = *cit;

            QHash<QString, QContact>::Iterator existing = existingContacts.find(address);
            if (existing == existingContacts.end()) {
                qCWarning(lcContactsd) << SRC_LOC << "No contact found for address:" << address;
                existing = existingContacts.insert(address, QContact());
                changes |= CDTpContact::All;
            }

            // If we got a contact without avatar in the roster, and the original
            // had an avatar, then ignore the avatar update (some contact managers
            // send the initial roster with the avatar missing)
            // Contact updates that have a null avatar will clear the avatar though
            if (changes & CDTpContact::DefaultAvatar) {
                if (((changes & CDTpContact::All) != CDTpContact::All) &&
                    contactWrapper->contact()->avatarData().fileName.isEmpty()) {
                    changes &= ~CDTpContact::DefaultAvatar;
                }
            }

            updateContactChanges(contactWrapper, changes, *existing, &saveSet, &removeList);
        }

        updateContacts(SRC_LOC, &saveSet, &removeList);
    } else {
        ContactChangeSet saveSet;

        const QContactPresence::PresenceState newState(qContactPresenceState(Tp::ConnectionPresenceTypeUnknown));
        const QStringList newCapabilities(currentCapabilites(account->capabilities(), Tp::ConnectionPresenceTypeUnknown, account));

        // Set presence to unknown for all contacts of this account
        QContactFetchHint hint(contactFetchHint(DetailList() << detailType<QContactPresence>()
                                                             << detailType<QContactOnlineAccount>()
                                                             << detailType<QContactOriginMetadata>()));
        foreach (QContact existing, manager()->contacts(findContactIdsForAccount(accountPath), hint)) {
            const QContactId &contactId(existing.id());

            CDTpContact::Changes changes;

            QContactPresence presence = existing.detail<QContactPresence>();

            if (presence.presenceState() != newState) {
                presence.setPresenceState(newState);
                presence.setTimestamp(QDateTime::currentDateTime());

                if (!storeContactDetail(existing, presence, SRC_LOC)) {
                    qCWarning(lcContactsd) << SRC_LOC << "Unable to save unknown presence to contact for:" << contactId;
                }

                changes |= CDTpContact::Presence;
            }

            // Also reset the capabilities
            QContactOnlineAccount qcoa = existing.detail<QContactOnlineAccount>();

            if (qcoa.capabilities() != newCapabilities || onlineAccountEnabled(qcoa)) {
                qcoa.setCapabilities(newCapabilities);
                qcoa.setValue(QContactOnlineAccount__FieldEnabled, asString(false));

                if (!storeContactDetail(existing, qcoa, SRC_LOC)) {
                    qCWarning(lcContactsd) << SRC_LOC << "Unable to save capabilities to contact for:" << contactId;
                }

                changes |= CDTpContact::Capabilities;
            }

            if (!account->isEnabled()) {
                // Mark the contact as un-enabled also
                QContactOriginMetadata metadata = existing.detail<QContactOriginMetadata>();

                if (metadata.enabled()) {
                    metadata.setEnabled(false);

                    if (!storeContactDetail(existing, metadata, SRC_LOC)) {
                        qCWarning(lcContactsd) << SRC_LOC << "Unable to un-enable contact for:" << contactId;
                    }

                    changes |= CDTpContact::Capabilities;
                }
            }

            appendContactChange(&saveSet, existing, changes);
        }

        updateContacts(SRC_LOC, &saveSet, 0);
    }
}

void CDTpStorage::syncAccounts(const QList<CDTpAccountPtr> &accounts)
{
    qWarning() << "CDTpStorage: syncAccounts:" << accounts.count();

    for (CDTpAccountPtr accountWrapper : accounts) {
        QContact self(selfContact(telepathyCollectionId(imAccount(accountWrapper))));
        if (self.isEmpty()) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to retrieve self contact - error:" << manager()->error();
            return;
        }
        syncAccountsForSelfContact(accounts, self);
    }
}

void CDTpStorage::syncAccountsForSelfContact(const QList<CDTpAccountPtr> &accounts, QContact &self)
{
    // Find the list of paths for the accounts we now have
    QStringList accountPaths = forEachItem(accounts, extractAccountPath);

    qWarning() << "CDTpStorage: syncAccountsForSelfContact:" << accountPaths;

    QSet<int> existingIndices;
    QSet<QString> removalPaths;

    foreach (QContactOnlineAccount existingAccount, self.details<QContactOnlineAccount>()) {
        const QString existingPath(stringValue(existingAccount, QContactOnlineAccount__FieldAccountPath));
        if (existingPath.isEmpty()) {
            qCWarning(lcContactsd) << SRC_LOC << "No path for existing account:" << existingPath;
            continue;
        }

        int index = accountPaths.indexOf(existingPath);
        if (index != -1) {
            existingIndices.insert(index);
            updateAccountChanges(self, existingAccount, accounts.at(index), CDTpAccount::All);
        } else {
            qCDebug(lcContactsd) << SRC_LOC << "Remove obsolete account:" << existingPath;

            // This account is no longer valid
            removalPaths.insert(existingPath);
        }
    }

    // Remove invalid accounts
    foreach (QContactOnlineAccount existingAccount, self.details<QContactOnlineAccount>()) {
        const QString existingPath(stringValue(existingAccount, QContactOnlineAccount__FieldAccountPath));
        if (removalPaths.contains(existingPath)) {
            removeExistingAccount(self, existingAccount);
        }
    }

    // Add any previously unknown accounts
    for (int i = 0; i < accounts.length(); ++i) {
        if (!existingIndices.contains(i)) {
            addNewAccount(self, accounts.at(i));
        }
    }

    storeSelfContact(mDevicePresence, self, SRC_LOC, CDTpContact::All, true);
}

void CDTpStorage::createAccount(CDTpAccountPtr accountWrapper)
{
    QContact self(selfContact(telepathyCollectionId(imAccount(accountWrapper))));
    if (self.isEmpty()) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to retrieve self contact:" << manager()->error();
        return;
    }

    const QString accountPath(imAccount(accountWrapper));

    qWarning() << "CDTpStorage: createAccount:" << accountPath;

    // Ensure this account does not already exist
    foreach (const QContactOnlineAccount &existingAccount, self.details<QContactOnlineAccount>()) {
        const QString existingPath(stringValue(existingAccount, QContactOnlineAccount__FieldAccountPath));
        if (existingPath == accountPath) {
            qCWarning(lcContactsd) << SRC_LOC << "Path already exists for create account:" << existingPath;
            return;
        }
    }

    // Add any previously unknown accounts
    addNewAccount(self, accountWrapper);

    QList<CDTpContactPtr> tpContacts(accountContacts(accountWrapper));

    QStringList contactAddresses;
    foreach (const CDTpContactPtr &contactWrapper, tpContacts) {
        const QString address = imAddress(accountPath, contactWrapper->contact()->id());
        contactAddresses.append(address);
    }

    // Retrieve the existing contacts in a single batch
    QHash<QString, QContact> existingContacts = findExistingContacts(
                contactAddresses, telepathyCollectionId(imAccount(accountWrapper)));

    ContactChangeSet saveSet;
    QList<QContactId> removeList;

    // Add any contacts already present for this account
    foreach (const CDTpContactPtr &contactWrapper, tpContacts) {
        const QString address = imAddress(accountPath, contactWrapper->contact()->id());

        QHash<QString, QContact>::Iterator existing = existingContacts.find(address);
        if (existing == existingContacts.end()) {
            qCWarning(lcContactsd) << SRC_LOC << "No contact found for address:" << address;
            existing = existingContacts.insert(address, QContact());
        }

        updateContactChanges(contactWrapper, CDTpContact::All, *existing, &saveSet, &removeList);
    }

    updateContacts(SRC_LOC, &saveSet, &removeList);
}

void CDTpStorage::updateAccount(CDTpAccountPtr accountWrapper, CDTpAccount::Changes changes)
{
    QContact self(selfContact(telepathyCollectionId(imAccount(accountWrapper))));
    if (self.isEmpty()) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to retrieve self contact:" << manager()->error();
        return;
    }

    const QString accountPath(imAccount(accountWrapper));

    qWarning() << "CDTpStorage: updateAccount:" << accountPath << asString(changes);

    foreach (QContactOnlineAccount existingAccount, self.details<QContactOnlineAccount>()) {
        const QString existingPath(stringValue(existingAccount, QContactOnlineAccount__FieldAccountPath));
        if (existingPath == accountPath) {
            updateAccountChanges(self, existingAccount, accountWrapper, changes);
            return;
        }
    }

    qCWarning(lcContactsd) << SRC_LOC << "Account not found for update account:" << accountPath;
}

void CDTpStorage::removeAccount(CDTpAccountPtr accountWrapper)
{
    cancelQueuedUpdates(accountContacts(accountWrapper));

    QContact self(selfContact(telepathyCollectionId(imAccount(accountWrapper))));
    if (self.isEmpty()) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to retrieve self contact:" << manager()->error();
        return;
    }
    QContact globalSelf = manager()->contact(manager()->selfContactId());

    const QString accountPath(imAccount(accountWrapper));

    qWarning() << "CDTpStorage: removeAccount:" << accountPath;

    foreach (QContactOnlineAccount existingAccount, self.details<QContactOnlineAccount>()) {
        const QString existingPath(stringValue(existingAccount, QContactOnlineAccount__FieldAccountPath));
        if (existingPath == accountPath) {
            removeExistingAccount(self, existingAccount);

            emitAccountChanges(mDevicePresence, globalSelf, true);
            return;
        }
    }

    qCWarning(lcContactsd) << SRC_LOC << "Account not found for remove account:" << accountPath;
}

// This is called when account goes online/offline
void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper)
{
    QContact self(selfContact(telepathyCollectionId(imAccount(accountWrapper))));
    if (self.isEmpty()) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to retrieve self contact:" << manager()->error();
        return;
    }

    const QString accountPath(imAccount(accountWrapper));

    qWarning() << "CDTpStorage: syncAccountContacts (roster change):" << accountPath;

    foreach (QContactOnlineAccount existingAccount, self.details<QContactOnlineAccount>()) {
        const QString existingPath(stringValue(existingAccount, QContactOnlineAccount__FieldAccountPath));
        if (existingPath == accountPath) {
            updateAccountChanges(self, existingAccount, accountWrapper, CDTpAccount::Enabled);
            return;
        }
    }

    qCWarning(lcContactsd) << SRC_LOC << "Account not found for sync account:" << accountPath;
}

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper, const QList<CDTpContactPtr> &contactsAdded, const QList<CDTpContactPtr> &contactsRemoved)
{
    const QString accountPath(imAccount(accountWrapper));

    qWarning() << "CDTpStorage: syncAccountContacts (roster update):" << accountPath << contactsAdded.count() << contactsRemoved.count();

    // Ensure there are no duplicates in the list
    QList<CDTpContactPtr> addedContacts(contactsAdded.toSet().toList());
    QList<CDTpContactPtr> removedContacts(contactsRemoved.toSet().toList());

    QSet<QString> contactAddresses;
    foreach (const CDTpContactPtr &contactWrapper, addedContacts) {
        // This contact must be for the specified account
        if (imAccount(contactWrapper) != accountPath) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to add contact from wrong account:" << imAccount(contactWrapper) << accountPath;
            continue;
        }

        const QString address = imAddress(accountPath, contactWrapper->contact()->id());
        contactAddresses.insert(address);
    }
    foreach (const CDTpContactPtr &contactWrapper, removedContacts) {
        if (imAccount(contactWrapper) != accountPath) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to remove contact from wrong account:" << imAccount(contactWrapper) << accountPath;
            continue;
        }

        const QString address = imAddress(accountPath, contactWrapper->contact()->id());
        contactAddresses.insert(address);
    }

    // Retrieve the existing contacts in a single batch
    QHash<QString, QContact> existingContacts = findExistingContacts(
                contactAddresses, telepathyCollectionId(imAccount(accountWrapper)));

    ContactChangeSet saveSet;
    QList<QContactId> removeList;

    foreach (const CDTpContactPtr &contactWrapper, addedContacts) {
        const QString address = imAddress(accountPath, contactWrapper->contact()->id());

        CDTpContact::Changes changes = CDTpContact::Information;

        QHash<QString, QContact>::Iterator existing = existingContacts.find(address);
        if (existing == existingContacts.end()) {
            qCWarning(lcContactsd) << SRC_LOC << "No contact found for address:" << address;
            existing = existingContacts.insert(address, QContact());
            changes |= CDTpContact::All;
        }

        updateContactChanges(contactWrapper, changes, *existing, &saveSet, &removeList);
    }
    foreach (const CDTpContactPtr &contactWrapper, removedContacts) {
        const QString address = imAddress(accountPath, contactWrapper->contact()->id());

        QHash<QString, QContact>::Iterator existing = existingContacts.find(address);
        if (existing == existingContacts.end()) {
            qCWarning(lcContactsd) << SRC_LOC << "No contact found for address:" << address;
            continue;
        }

        updateContactChanges(contactWrapper, CDTpContact::Deleted, *existing, &saveSet, &removeList);
    }

    updateContacts(SRC_LOC, &saveSet, &removeList);
}

void CDTpStorage::createAccountContacts(CDTpAccountPtr accountWrapper, const QStringList &imIds, uint localId)
{
    Q_UNUSED(localId) // ???

    const QString accountPath(imAccount(accountWrapper));

    qWarning() << "CDTpStorage: createAccountContacts:" << accountPath << imIds.count();

    ContactChangeSet saveSet;

    foreach (const QString &id, imIds) {
        QContact newContact;
        if (!initializeNewContact(newContact, accountWrapper, id, QString())) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to create contact for account:" << accountPath << id;
        } else {
            appendContactChange(&saveSet, newContact, CDTpContact::All);
        }
    }

    updateContacts(SRC_LOC, &saveSet, 0);
}

/* Use this only in offline mode - use syncAccountContacts in online mode */
void CDTpStorage::removeAccountContacts(CDTpAccountPtr accountWrapper, const QStringList &contactIds)
{
    const QString accountPath(imAccount(accountWrapper));

    qWarning() << "CDTpStorage: removeAccountContacts:" << accountPath << contactIds.count();

    QStringList imAddressList;
    foreach (const QString &id, contactIds) {
        imAddressList.append(imAddress(accountPath, id));
    }

    QList<QContactId> removeIds;

    // Find any contacts matching the supplied ID list
    QContactFetchHint hint(contactFetchHint(DetailList() << detailType<QContactOriginMetadata>()));
    foreach (const QContact &existing, manager()->contacts(findContactIdsForAccount(accountPath), hint)) {
        QContactOriginMetadata metadata = existing.detail<QContactOriginMetadata>();
        if (imAddressList.contains(metadata.id())) {
            removeIds.append(existing.id());
        }
    }

    if (!manager()->removeContacts(removeIds)) {
        qCWarning(lcContactsd) << SRC_LOC << "Unable to remove contacts for account:" << accountPath << "error:" << manager()->error();
    }
}

void CDTpStorage::updateContact(CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    mUpdateQueue[contactWrapper] |= changes;

    // Only update IM contacts after not receiving an update notification for the defined period
    // Also use an upper limit to keep latency within acceptable bounds.
    if (mWaitTimer.isValid()) {
        if (mWaitTimer.elapsed() >= UPDATE_MAXIMUM_TIMEOUT) {
            // Don't prolong the wait any further
            return;
        }
    } else {
        mWaitTimer.start();
    }

    mUpdateTimer.start();
}

void CDTpStorage::onUpdateQueueTimeout()
{
    mWaitTimer.invalidate();

    qCDebug(lcContactsd) << "Update" << mUpdateQueue.count() << "contacts";

    QHash<CDTpContactPtr, CDTpContact::Changes> updates;
    QSet<QString> contactAddresses;

    QHash<CDTpContactPtr, CDTpContact::Changes>::const_iterator it = mUpdateQueue.constBegin(), end = mUpdateQueue.constEnd();
    for ( ; it != end; ++it) {
        CDTpContactPtr contactWrapper = it.key();

        // If there are multiple entries for a contact, coalesce the changes
        updates[contactWrapper] |= it.value();
        contactAddresses.insert(imAddress(contactWrapper));
    }

    mUpdateQueue.clear();

    // Retrieve the existing contacts
    QHash<QString, QContact> existingContacts;
    const QList<QContactCollection> telepathyCollections = allTelepathyCollections();
    for (const QContactCollection &collection : telepathyCollections) {
        existingContacts.unite(findExistingContacts(contactAddresses, collection.id()));
    }

    ContactChangeSet saveSet;
    QList<QContactId> removeList;

    for (it = updates.constBegin(), end = updates.constEnd(); it != end; ++it) {
        CDTpContactPtr contactWrapper = it.key();

        // Skip the contact in case its account was deleted before this function
        // was invoked
        if (contactWrapper->accountWrapper().isNull()) {
            continue;
        }
        if (!contactWrapper->isVisible()) {
            continue;
        }

        const QString address(imAddress(contactWrapper));
        CDTpContact::Changes changes = it.value();

        QHash<QString, QContact>::Iterator existing = existingContacts.find(address);
        if (existing == existingContacts.end()) {
            qCWarning(lcContactsd) << SRC_LOC << "No contact found for address:" << address;
            existing = existingContacts.insert(address, QContact());
            changes |= CDTpContact::All;
        }

        updateContactChanges(contactWrapper, changes, *existing, &saveSet, &removeList);
    }

    updateContacts(SRC_LOC, &saveSet, &removeList);
}

void CDTpStorage::cancelQueuedUpdates(const QList<CDTpContactPtr> &contacts)
{
    foreach (const CDTpContactPtr &contactWrapper, contacts) {
        mUpdateQueue.remove(contactWrapper);
    }
}

void CDTpStorage::reportPresenceStates()
{
    const QList<QContactCollection> telepathyCollections = allTelepathyCollections();

    for (const QContactCollection &collection : telepathyCollections) {
        QContact self(selfContact(collection.id()));
        if (self.isEmpty()) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to retrieve self contact - error:" << manager()->error();
        } else {
            reportPresenceState(self);
        }
    }
}

void CDTpStorage::reportPresenceState(QContact &self)
{
    emit mDevicePresence->globalUpdate(self.detail<QContactGlobalPresence>().presenceState());

    QStringList accountPaths;
    foreach (const QContactOnlineAccount &qcoa, self.details<QContactOnlineAccount>()) {
        QContactPresence presence(findPresenceForAccount(self, qcoa));
        if (presence.isEmpty()) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to find presence to match account:" << qcoa.detailUri();
        }
        reportAccountPresence(mDevicePresence, qcoa, presence);

        accountPaths.append(qcoa.value<QString>(QContactOnlineAccount__FieldAccountPath));
    }

    emit mDevicePresence->accountList(accountPaths);

    // Retrieve the aggregate self contact to report name details
    self = selfDetailsContact();
    reportSelfDetails(mDevicePresence, self, mDisplayLabelOrder);
}

void CDTpStorage::displayLabelOrderChanged()
{
    QVariant displayLabelOrder = mDisplayLabelOrderConf.value();
    if (displayLabelOrder.isValid() && displayLabelOrder.toInt() != mDisplayLabelOrder) {
        mDisplayLabelOrder = static_cast<DisplayLabelOrder>(displayLabelOrder.toInt());

        // Update our self details
        QContact self(selfDetailsContact());
        if (self.isEmpty()) {
            qCWarning(lcContactsd) << SRC_LOC << "Unable to retrieve self contact - error:" << manager()->error();
            return;
        }

        reportSelfDetails(mDevicePresence, self, mDisplayLabelOrder);
    }
}

// Instantiate the QContactOriginMetadata functions
#include <qcontactoriginmetadata_impl.h>
