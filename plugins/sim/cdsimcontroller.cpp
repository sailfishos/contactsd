/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2013-2019 Jolla Ltd.
 ** Copyright (c) 2020 Open Mobile Platform LLC.
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **/

#include "cdsimcontroller.h"
#include "cdsimplugin.h"
#include "debug.h"

#include <qtcontacts-extensions.h>
#include <qtcontacts-extensions_manager_impl.h>
#include <contactmanagerengine.h>
#include <QContactDeactivated>
#include <QContactStatusFlags>

#include <QContactDetailFilter>
#include <QContactIntersectionFilter>
#include <QContactCollectionFilter>
#include <QContactNickname>
#include <QContactPhoneNumber>
#include <QContactTag>

#include <QVersitContactImporter>

using namespace Contactsd;

namespace {

const QString CollectionKeyModemPath = QStringLiteral("ModemPath");
const QString CollectionKeyModemIdentifier = QStringLiteral("ModemIdentifier");

QMap<QString, QString> contactManagerParameters()
{
    QMap<QString, QString> rv;
    rv.insert(QStringLiteral("mergePresenceChanges"), QStringLiteral("false"));
    return rv;
}

}

CDSimModemData::CDSimModemData(CDSimController *controller, const QString &modemPath)
    : QObject(controller)
    , m_modemPath(modemPath)
    , m_voicemailConf(0)
    , m_ready(false)
    , m_retries(0)
{
    connect(&m_simManager, SIGNAL(presenceChanged(bool)), SLOT(simStateChanged()));
    connect(&m_simManager, SIGNAL(cardIdentifierChanged(QString)), SLOT(simStateChanged()));

    connect(&m_phonebook, SIGNAL(importReady(QString)), SLOT(vcardDataAvailable(QString)));
    connect(&m_phonebook, SIGNAL(importFailed()), SLOT(vcardReadFailed()));

    // Resync the contacts list whenever the phonebook availability changes
    connect(&m_phonebook, SIGNAL(validChanged(bool)), SLOT(phonebookValidChanged(bool)));

    connect(&m_contactReader, SIGNAL(stateChanged(QVersitReader::State)), SLOT(readerStateChanged(QVersitReader::State)));

    connect(&m_messageWaiting, SIGNAL(voicemailMailboxNumberChanged(QString)), SLOT(voicemailConfigurationChanged()));

    // Don't activate ofono objects in test mode
    if (controller->m_active) {
        // Start reading the SIM contacts
        m_simManager.setModemPath(m_modemPath);
        m_simInfo.setModemPath(m_modemPath);
        m_phonebook.setModemPath(m_modemPath);
        m_messageWaiting.setModemPath(m_modemPath);
    }
}

CDSimModemData::~CDSimModemData()
{
    delete m_voicemailConf;
}

CDSimController *CDSimModemData::controller() const
{
    return qobject_cast<CDSimController *>(parent());
}

QContactManager &CDSimModemData::manager() const
{
    return controller()->contactManager();
}

QString CDSimModemData::modemIdentifier() const
{
    if (controller()->m_active) {
        return m_simManager.cardIdentifier();
    }
    return m_modemPath;
}

QString CDSimModemData::modemPath() const
{
    return m_modemPath;
}

QContactCollection CDSimModemData::contactCollection() const
{
    return m_collection;
}

QList<QContact> CDSimModemData::fetchContacts() const
{
    QContactCollectionFilter filter;
    filter.setCollectionId(m_collection.id());

    QContactFetchHint hint;
    hint.setOptimizationHints(QContactFetchHint::NoRelationships | QContactFetchHint::NoActionPreferences | QContactFetchHint::NoBinaryBlobs);

    return manager().contacts(filter, QList<QContactSortOrder>(), hint);
}

bool CDSimModemData::ready() const
{
    return m_ready;
}

void CDSimModemData::setReady(bool ready)
{
    if (m_ready != ready) {
        m_ready = ready;
        emit readyChanged(m_ready);

        if (m_ready) {
            initCollection();
            updateVoicemailConfiguration();

            if (m_phonebook.isValid()) {
                performTransientImport();
            }
        }
    }
}

void CDSimModemData::updateBusy()
{
    controller()->updateBusy();
}

void CDSimModemData::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_retryTimer.timerId()) {
        m_retryTimer.stop();

        if (m_ready) {
            performTransientImport();
        }
    }
}


CDSimController::CDSimController(QObject *parent, bool active)
    : QObject(parent)
    , m_manager(QStringLiteral("org.nemomobile.contacts.sqlite"), contactManagerParameters())
    , m_transientImport(true)
    , m_busy(false)
    , m_active(active)
    , m_transientImportConf(QString::fromLatin1("/org/nemomobile/contacts/sim/transient_import"))
{
    QVariant transientImport = m_transientImportConf.value();
    if (transientImport.isValid())
        m_transientImport = (transientImport.toInt() == 1);

    connect(&m_transientImportConf, SIGNAL(valueChanged()), this, SLOT(transientImportConfigurationChanged()));
}

CDSimController::~CDSimController()
{
}

static QContactFilter deactivatedFilter()
{
    return QContactStatusFlags::matchFlag(QContactStatusFlags::IsDeactivated, QContactFilter::MatchContains);
}

QContactManager &CDSimController::contactManager()
{
    return m_manager;
}

QContactCollection CDSimController::contactCollection(const QString &modemPath) const
{
    for (auto it = m_modems.constBegin(); it != m_modems.constEnd(); ++it) {
        CDSimModemData *data = it.value();
        if (data->modemPath() == modemPath) {
            return data->contactCollection();
        }
    }

    return QContactCollection();
}

int CDSimController::modemIndex(const QString &modemPath) const
{
    return m_availableModems.indexOf(modemPath);
}

void CDSimController::setModemPaths(const QStringList &paths)
{
    qWarning() << "Managing SIM contacts for modem paths:" << paths;

    m_availableModems = paths;

    QMap<QString, CDSimModemData *>::iterator it = m_modems.begin();
    while (it != m_modems.end()) {
        if (paths.contains(it.key())) {
            ++it;
        } else {
            // This modem is no longer active; remove it
            delete it.value();
            it = m_modems.erase(it);
        }
    }

    QSet<QString> absentModemPaths;

    foreach (const QString &path, paths) {
        CDSimModemData *modemData = m_modems.value(path);
        if (!modemData) {
            modemData = new CDSimModemData(this, path);
            connect(modemData, SIGNAL(readyChanged(bool)), SLOT(modemReadyChanged(bool)));
            m_modems.insert(path, modemData);
        }

        if (!modemData->ready()) {
            absentModemPaths.insert(path);
        }
    }

    if (absentModemPaths.isEmpty()) {
        // Remove any SIM contacts that don't originate from our current modems
        removeObsoleteSimCollections();
    } else {
        // Check for obsolete contacts after all modems are ready
        m_absentModemPaths = absentModemPaths;
        m_readyTimer.start(30 * 1000, this);
    }
}

bool CDSimController::busy() const
{
    return m_busy;
}

void CDSimController::updateBusy()
{
    bool busy = false;
    QMap<QString, CDSimModemData *>::const_iterator mit = m_modems.constBegin(), mend = m_modems.constEnd();
    for ( ; !busy && mit != mend; ++mit) {
        busy |= ((*mit)->m_phonebook.importing() || (*mit)->m_contactReader.state() == QVersitReader::ActiveState);
    }

    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged(m_busy);
    }
}

void CDSimController::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_readyTimer.timerId()) {
        m_readyTimer.stop();

        if (!m_absentModemPaths.isEmpty()) {
            // Stop waiting for these modems to report presence
            m_absentModemPaths.clear();
            removeObsoleteSimCollections();
        }
    }
}

void CDSimModemData::performTransientImport()
{
    if (modemIdentifier().isEmpty()) {
        qWarning() << "No identifier available for modem:" << m_simManager.modemPath();
        return;
    }

    if (m_phonebook.isValid() && controller()->m_transientImport) {
        // Read all contacts from the SIM
        m_phonebook.beginImport();
    } else {
        m_simContacts.clear();
        deactivateAllSimContacts();
    }

    updateBusy();
}

void CDSimController::transientImportConfigurationChanged()
{
    bool importEnabled(true);

    QVariant transientImport = m_transientImportConf.value();
    if (transientImport.isValid())
        importEnabled = (transientImport.toInt() == 1);

    if (m_transientImport != importEnabled) {
        m_transientImport = importEnabled;

        QMap<QString, CDSimModemData *>::const_iterator mit = m_modems.constBegin(), mend = m_modems.constEnd();
        for ( ; mit != mend; ++mit) {
            (*mit)->performTransientImport();
        }
    }
}

void CDSimController::modemReadyChanged(bool ready)
{
    CDSimModemData *modem = qobject_cast<CDSimModemData *>(sender());
    if (ready && m_absentModemPaths.contains(modem->m_simManager.modemPath())) {
        m_absentModemPaths.remove(modem->m_simManager.modemPath());
        if (m_absentModemPaths.isEmpty()) {
            m_readyTimer.stop();

            // Remove any SIM contacts that don't originate from our current modems
            removeObsoleteSimCollections();
        }
    }
}

void CDSimModemData::simStateChanged()
{
    setReady(m_simManager.present() && !m_simManager.cardIdentifier().isEmpty());
}

void CDSimModemData::phonebookValidChanged(bool)
{
    if (m_ready) {
        performTransientImport();
    }
}

void CDSimModemData::vcardDataAvailable(const QString &vcardData)
{
    // Create contact records from the SIM VCard data
    m_simContacts.clear();
    m_contactReader.setData(vcardData.toUtf8());
    m_contactReader.startReading();
    updateBusy();

    m_retries = 0;
}

void CDSimModemData::vcardReadFailed()
{
    qWarning() << "Unable to read VCard data from SIM:" << m_phonebook.modemPath();
    updateBusy();

    const int maxRetries = 5;
    if (m_retries < maxRetries) {
        ++m_retries;

        // Wait a short period before trying again; the phonebook often reports
        // a failure to read contact data on attempts early after boot
        m_retryTimer.start(10 * 1000, this);
    }
}

void CDSimModemData::readerStateChanged(QVersitReader::State state)
{
    if (state != QVersitReader::FinishedState)
        return;

    QList<QVersitDocument> results = m_contactReader.results();

    if (results.isEmpty()) {
        m_simContacts.clear();
        removeAllSimContacts();
    } else {
        QVersitContactImporter importer;
        importer.importDocuments(results);
        m_simContacts = importer.contacts();
        if (m_simContacts.isEmpty()) {
            removeAllSimContacts();
        } else {
            // import or remove contacts from local storage as necessary.
            ensureSimContactsPresent();
        }
    }

    updateBusy();
}

void CDSimModemData::deactivateAllSimContacts()
{
    const QList<QContact> contacts = fetchContacts();
    if (contacts.size()) {
        QList<QContact> deactivatedContacts;

        foreach (QContact contact, contacts) {
            QContactDeactivated deactivated;
            contact.saveDetail(&deactivated);
            deactivatedContacts.append(contact);
        }

        if (!manager().saveContacts(&deactivatedContacts)) {
            qWarning() << "Error deactivating sim contacts";
        }
    }
}

void CDSimModemData::removeAllSimContacts()
{
    if (m_collection.id().isNull()) {
        return;
    }

    QContactCollectionFilter collectionFilter;
    collectionFilter.setCollectionId(m_collection.id());

    const QList<QContactId> contactIds = manager().contactIds(collectionFilter, QList<QContactSortOrder>());
    if (contactIds.count()) {
        if (manager().removeContacts(contactIds)) {
            qDebug() << "Removed sim contacts for modem" << m_modemPath;
        } else {
            qWarning() << "Unable to remove sim contacts for modem" << m_modemPath;
        }
    }
}

void CDSimModemData::ensureSimContactsPresent()
{
    // Ensure all contacts from the SIM are present in the store
    QContactFetchHint hint;
    hint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactNickname::Type << QContactPhoneNumber::Type);
    hint.setOptimizationHints(QContactFetchHint::NoRelationships | QContactFetchHint::NoActionPreferences | QContactFetchHint::NoBinaryBlobs);

    QContactCollectionFilter collectionFilter;
    collectionFilter.setCollectionId(m_collection.id());

    QList<QContact> storedSimContacts = manager().contacts(collectionFilter, QList<QContactSortOrder>(), hint);

    // Also find any deactivated SIM contacts
    storedSimContacts.append(manager().contacts(collectionFilter & deactivatedFilter(), QList<QContactSortOrder>(), hint));

    QMap<QString, QContact> existingContacts;
    foreach (const QContact &contact, storedSimContacts) {
        // Identify imported SIM contacts by their nickname record
        const QString nickname(contact.detail<QContactNickname>().nickname().trimmed());
        existingContacts.insert(nickname, contact);
    }

    // coalesce SIM contacts with the same display label.
    QList<QContact> coalescedSimContacts;
    foreach (const QContact &simContact, m_simContacts) {
        const QContactDisplayLabel &displayLabel = simContact.detail<QContactDisplayLabel>();
        const QList<QContactPhoneNumber> &phoneNumbers = simContact.details<QContactPhoneNumber>();

        // search for a pre-existing match in the coalesced list.
        bool coalescedContactFound = false;
        for (int i = 0; i < coalescedSimContacts.size(); ++i) {
            QContact coalescedContact = coalescedSimContacts.at(i);
            if (coalescedContact.detail<QContactDisplayLabel>().label().trimmed() == displayLabel.label().trimmed()) {
                // found a match.  Coalesce the phone numbers and update the contact in the list.
                QList<QContactPhoneNumber> coalescedPhoneNumbers = coalescedContact.details<QContactPhoneNumber>();
                foreach (QContactPhoneNumber phn, phoneNumbers) {
                    // check to see if the coalesced phone numbers list contains this number
                    bool coalescedNumberFound = false;
                    foreach (const QContactPhoneNumber &cphn, coalescedPhoneNumbers) {
                        if (phn.number() == cphn.number()
                                && phn.contexts() == cphn.contexts()
                                && phn.subTypes() == cphn.subTypes()) {
                            // already exists in the coalesced contact, no need to add it.
                            coalescedNumberFound = true;
                            break;
                        }
                    }

                    // if not, add the number to the coalesced contact and to the coalesced list
                    if (!coalescedNumberFound) {
                        coalescedContact.saveDetail(&phn);
                        coalescedPhoneNumbers.append(phn);
                    }
                }
                coalescedSimContacts.replace(i, coalescedContact);
                coalescedContactFound = true;
                break;
            }
        }

        // no match? add to list.
        if (!coalescedContactFound) {
            coalescedSimContacts.append(simContact);
        }
    }

    QList<QContact> importContacts;
    foreach (QContact simContact, coalescedSimContacts) {
        // SIM imports have their name in the display label
        QContactDisplayLabel displayLabel = simContact.detail<QContactDisplayLabel>();

        // first, remove any duplicate phone number details from the sim contact
        QList<QContactPhoneNumber> existingSimPhoneNumbers;
        foreach (QContactPhoneNumber phoneNumber, simContact.details<QContactPhoneNumber>()) {
            bool foundDuplicateNumber = false;
            for (int i = 0; i < existingSimPhoneNumbers.size(); ++i) {
                if (existingSimPhoneNumbers.at(i).number() == phoneNumber.number()
                        && existingSimPhoneNumbers.at(i).contexts() == phoneNumber.contexts()
                        && existingSimPhoneNumbers.at(i).subTypes() == phoneNumber.subTypes()) {
                    // an exact duplicate of this number already exists in the sim contact.
                    foundDuplicateNumber = true;
                    simContact.removeDetail(&phoneNumber);
                    break;
                }
            }

            if (!foundDuplicateNumber) {
                existingSimPhoneNumbers.append(phoneNumber);
            }
        }

        // then, determine whether this contact is already represented in the device phonebook
        QMap<QString, QContact>::iterator it = existingContacts.find(displayLabel.label().trimmed());
        if (it != existingContacts.end()) {
            // Ensure this contact has the right phone numbers
            QContact &dbContact(*it);

            QList<QContactPhoneNumber> existingNumbers(dbContact.details<QContactPhoneNumber>());
            bool modified = false;
            foreach (QContactPhoneNumber phoneNumber, simContact.details<QContactPhoneNumber>()) {
                bool foundExistingNumber = false;
                for (int i = 0; i < existingNumbers.size(); ++i) {
                    if (existingNumbers.at(i).number() == phoneNumber.number()
                            && existingNumbers.at(i).contexts() == phoneNumber.contexts()
                            && existingNumbers.at(i).subTypes() == phoneNumber.subTypes()) {
                        // this number was not modified.  We don't need to change it.
                        foundExistingNumber = true;
                        existingNumbers.removeAt(i);
                        break;
                    }
                }

                if (!foundExistingNumber) {
                    // this number is new, or modified.  We need to change it.
                    dbContact.saveDetail(&phoneNumber);
                    modified = true;
                }
            }

            // Remove any obsolete numbers
            foreach (QContactPhoneNumber phoneNumber, existingNumbers) {
                dbContact.removeDetail(&phoneNumber);
                modified = true;
            }

            // Reactivate this contact if necessary
            if (!dbContact.details<QContactDeactivated>().isEmpty()) {
                QContactDeactivated deactivated = dbContact.detail<QContactDeactivated>();
                dbContact.removeDetail(&deactivated);
                modified = true;
            }

            if (modified) {
                // Add the modified contact to the import set
                importContacts.append(dbContact);
            }
            existingContacts.erase(it);
        } else {
            // We need to import this contact
            simContact.setCollectionId(m_collection.id());

            // Convert the display label to a nickname; display label is managed by the backend
            QContactNickname nickname = simContact.detail<QContactNickname>();
            nickname.setNickname(displayLabel.label().trimmed());
            simContact.saveDetail(&nickname);
            simContact.removeDetail(&displayLabel);

            importContacts.append(simContact);
        }
    }

    if (!importContacts.isEmpty()) {
        // Import any contacts which were modified or are not currently present
        if (!manager().saveContacts(&importContacts)) {
            qWarning() << "Error while saving imported sim contacts";
        }
    }

    if (!existingContacts.isEmpty()) {
        // Remove any imported contacts no longer on the SIM
        QList<QContactId> obsoleteIds;
        foreach (const QContact &contact, existingContacts.values()) {
            obsoleteIds.append(contact.id());
        }

        if (!manager().removeContacts(obsoleteIds)) {
            qWarning() << "Error while removing obsolete sim contacts";
        }
    }
}

void CDSimModemData::voicemailConfigurationChanged()
{
    if (!m_voicemailConf || !m_simManager.present()) {
        // Wait until SIM is present
        return;
    }

    const QString voicemailTarget(QString::fromLatin1("voicemail"));

    QContactCollectionFilter collectionFilter;
    collectionFilter.setCollectionId(m_collection.id());

    QContactIntersectionFilter filter;
    filter << collectionFilter;
    filter << QContactTag::match(voicemailTarget);

    QContact voicemailContact;
    for (const QContact &contact : manager().contacts(filter)) {
        voicemailContact = contact;
        break;
    }

    // If there is a manually configured number, prefer that
    QString voicemailNumber(m_voicemailConf->value().toString());
    if (voicemailNumber.isEmpty()) {
        // Otherwise use the number provided for message waiting
        voicemailNumber = m_messageWaiting.voicemailMailboxNumber();
    }

    if (voicemailNumber.isEmpty()) {
        // Remove the voicemail contact if present
        if (!voicemailContact.id().isNull()) {
            if (!manager().removeContact(voicemailContact.id())) {
                qWarning() << "Unable to remove voicemail contact";
            }
        }
    } else {
        // Add/update the voicemail contact if necessary
        QContactPhoneNumber number = voicemailContact.detail<QContactPhoneNumber>();
        if (number.number() == voicemailNumber) {
            // Nothing to change
            return;
        }

        // Update the number
        number.setNumber(voicemailNumber);
        voicemailContact.saveDetail(&number);

        QContactNickname nickname = voicemailContact.detail<QContactNickname>();
        if (nickname.isEmpty()) {
            //: Name for the contact representing the voicemail mailbox
            //% "Voicemail System"
            QString name(qtTrId("qtn_sim_voicemail_contact"));

            const QString simName(m_simInfo.serviceProviderName());
            if (!simName.isEmpty()) {
                name.append(QString(QStringLiteral(" (%1)")).arg(simName));
            }

            nickname.setNickname(name);
            voicemailContact.saveDetail(&nickname);
        }

        const QList<QContactTag> tags = voicemailContact.details<QContactTag>();
        bool foundTag = false;
        for (const QContactTag &tag : tags) {
            if (tag.tag() == voicemailTarget) {
                foundTag = true;
            }
        }
        if (!foundTag) {
            QContactTag tag;
            tag.setTag(voicemailTarget);
            voicemailContact.saveDetail(&tag);
        }

        voicemailContact.setCollectionId(m_collection.id());

        if (!manager().saveContact(&voicemailContact)) {
            qWarning() << "Unable to save voicemail contact";
        }
    }
}

void CDSimModemData::updateVoicemailConfiguration()
{
    QString variablePath(QString::fromLatin1("/sailfish/voicecall/voice_mailbox/"));
    variablePath.append(modemIdentifier());

    if (!m_voicemailConf || m_voicemailConf->key() != variablePath) {
        delete m_voicemailConf;
        m_voicemailConf = new MGConfItem(variablePath);
        connect(m_voicemailConf, SIGNAL(valueChanged()), this, SLOT(voicemailConfigurationChanged()));

        voicemailConfigurationChanged();
    }
}

void CDSimController::removeObsoleteSimCollections()
{
    QList<QContactCollectionId> obsoleteCollectionIds;

    const QList<QContactCollection> collections = contactManager().collections();

    QSet<QString> currentModemPaths;
    for (CDSimModemData *modem : m_modems.values()) {
        currentModemPaths.insert(modem->modemPath());
    }

    QSet<QString> matchedModemPaths;
    for (const QContactCollection &collection : collections) {
        const QString modemPath = collection.extendedMetaData(CollectionKeyModemPath).toString();
        if (modemPath.isEmpty()) {
            continue;
        }

        if (matchedModemPaths.contains(modemPath)) {
            obsoleteCollectionIds.append(collection.id());
        } else {
            if (currentModemPaths.contains(modemPath)) {
                matchedModemPaths.insert(modemPath);
            } else {
                obsoleteCollectionIds.append(collection.id());
            }
        }
    }

    if (!obsoleteCollectionIds.isEmpty()
            && !CDSimModemData::removeCollections(&contactManager(), obsoleteCollectionIds)) {
        qWarning() << "Error removing sim contacts for collections:" << obsoleteCollectionIds;
    }
}

void CDSimModemData::initCollection()
{
    const int modemIndex = controller()->modemIndex(m_modemPath);
    QString icon;
    if (modemIndex == 0) {
        icon = QStringLiteral("image://theme/icon-m-sim-1");
    } else if (modemIndex == 1) {
        icon = QStringLiteral("image://theme/icon-m-sim-2");
    }

    QContactCollection collection;
    collection.setMetaData(QContactCollection::KeyName, QStringLiteral("SIM"));
    collection.setMetaData(QContactCollection::KeyDescription, QStringLiteral("Contacts stored on SIM card"));
    collection.setMetaData(QContactCollection::KeyColor, QStringLiteral("green"));
    collection.setMetaData(QContactCollection::KeySecondaryColor, QStringLiteral("lightgreen"));
    collection.setMetaData(QContactCollection::KeyImage, icon);
    collection.setExtendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_APPLICATIONNAME, qApp->applicationName());
    collection.setExtendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_READONLY, true);
    collection.setExtendedMetaData(CollectionKeyModemPath, m_modemPath);
    collection.setExtendedMetaData(CollectionKeyModemIdentifier, modemIdentifier());

    const QList<QContactCollection> collections = manager().collections();
    for (const QContactCollection &c : collections) {
        const QString modemPath = c.extendedMetaData(CollectionKeyModemPath).toString();
        const QString modemIdentifier = c.extendedMetaData(CollectionKeyModemIdentifier).toString();
        if (modemPath == m_modemPath && modemIdentifier == this->modemIdentifier()) {
            qDebug() << "Found existing SIM collection" << collection.id()
                     << "for path" << m_modemPath
                     << "and modem id" << modemIdentifier;
            m_collection = c;
            break;
        }
    }

    if (m_collection.id().isNull()) {
        if (manager().saveCollection(&collection)) {
            m_collection = collection;
            qDebug() << "Created new SIM collection" << collection.id()
                     << "for path" << m_modemPath
                     << "and modem id" << modemIdentifier();
        } else {
            qWarning() << "Unable to save SIM collection for path" << m_modemPath
                     << "and modem id" << modemIdentifier()
                     << "error is:" << manager().error();
        }
    }
}

bool CDSimModemData::removeCollections(QContactManager *manager, const QList<QContactCollectionId> &collectionIds)
{
    if (collectionIds.isEmpty()) {
        return true;
    }

    QContactManager &m = *manager;
    QtContactsSqliteExtensions::ContactManagerEngine *cme = QtContactsSqliteExtensions::contactManagerEngine(m);
    QContactManager::Error error = QContactManager::NoError;

    bool ret = cme->storeChanges(nullptr,
                                 nullptr,
                                 collectionIds,
                                 QtContactsSqliteExtensions::ContactManagerEngine::PreserveLocalChanges,
                                 true,
                                 &error);
    if (!ret) {
        qWarning() << "Error removing sim contacts from device storage, error was:" << error;
        return false;
    }

    return true;
}

// Instantiate the extension functions
#include <qcontactdeactivated_impl.h>
#include <qcontactstatusflags_impl.h>
