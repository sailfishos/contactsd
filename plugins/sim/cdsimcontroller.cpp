/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2013-2014 Jolla Ltd.
 **
 ** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
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

#include <QContactDetailFilter>
#include <QContactDeactivated>
#include <QContactNickname>
#include <QContactPhoneNumber>
#include <QContactSyncTarget>
#include <QContactStatusFlags>

#include <QVersitContactImporter>

using namespace Contactsd;

namespace {

QMap<QString, QString> contactManagerParameters()
{
    QMap<QString, QString> rv;
    rv.insert(QStringLiteral("mergePresenceChanges"), QStringLiteral("false"));
    return rv;
}

}

CDSimController::CDSimController(QObject *parent, const QString &syncTarget)
    : QObject(parent)
    , m_manager(QStringLiteral("org.nemomobile.contacts.sqlite"), contactManagerParameters())
    , m_transientImport(true)
    , m_simSyncTarget(syncTarget)
    , m_busy(false)
    , m_voicemailConf(0)
    , m_transientImportConf(QString::fromLatin1("/org/nemomobile/contacts/sim/transient_import"))
{
    QVariant transientImport = m_transientImportConf.value();
    if (transientImport.isValid())
        m_transientImport = (transientImport.toInt() == 1);

    connect(&m_transientImportConf, SIGNAL(valueChanged()), this, SLOT(transientImportConfigurationChanged()));

    connect(&m_phonebook, SIGNAL(importReady(QString)),
            this, SLOT(vcardDataAvailable(QString)));
    connect(&m_phonebook, SIGNAL(importFailed()),
            this, SLOT(vcardReadFailed()));

    connect(&m_messageWaiting, SIGNAL(voicemailMailboxNumberChanged(QString)),
            this, SLOT(voicemailConfigurationChanged()));

    connect(&m_contactReader, SIGNAL(stateChanged(QVersitReader::State)),
            this, SLOT(readerStateChanged(QVersitReader::State)));

    // Resync the contacts list whenever the phonebook availability changes
    connect(&m_phonebook, SIGNAL(validChanged(bool)),
            this, SLOT(phoneBookAvailabilityChanged(bool)));

    connect(&m_simManager, SIGNAL(presenceChanged(bool)),
            this, SLOT(simPresenceChanged(bool)));
}

CDSimController::~CDSimController()
{
}

QContactDetailFilter CDSimController::simSyncTargetFilter() const
{
    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(m_simSyncTarget);
    return syncTargetFilter;
}

QContactFilter CDSimController::deactivatedFilter() const
{
    return QContactStatusFlags::matchFlag(QContactStatusFlags::IsDeactivated, QContactFilter::MatchContains);
}

QContactManager &CDSimController::contactManager()
{
    return m_manager;
}

void CDSimController::setModemPath(const QString &path)
{
    qDebug() << "Using modem path:" << path;
    m_phonebook.setModemPath(path);
    m_messageWaiting.setModemPath(path);
    m_simManager.setModemPath(path);
}

bool CDSimController::busy() const
{
    return m_busy;
}

void CDSimController::updateBusy()
{
    bool busy = m_phonebook.importing() || m_contactReader.state() == QVersitReader::ActiveState;
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged(m_busy);
    }
}

void CDSimController::performTransientImport()
{
    if (m_simSyncTarget.isEmpty()) {
        qWarning() << "No sync target is configured";
        return;
    }

    if (m_phonebook.isValid() && m_transientImport) {
        // Read all contacts from the SIM
        m_phonebook.beginImport();
        updateBusy();
    } else {
        m_simContacts.clear();
        deactivateAllSimContacts();
    }
}

void CDSimController::transientImportConfigurationChanged()
{
    bool importEnabled(true);

    QVariant transientImport = m_transientImportConf.value();
    if (transientImport.isValid())
        importEnabled = (transientImport.toInt() == 1);

    if (m_transientImport != importEnabled) {
        m_transientImport = importEnabled;
        performTransientImport();
    }
}

void CDSimController::simPresenceChanged(bool)
{
    updateVoicemailConfiguration();
}

void CDSimController::phoneBookAvailabilityChanged(bool available)
{
    performTransientImport();
    updateBusy();
}

void CDSimController::vcardDataAvailable(const QString &vcardData)
{
    // Create contact records from the SIM VCard data
    m_simContacts.clear();
    m_contactReader.setData(vcardData.toUtf8());
    m_contactReader.startReading();
    updateBusy();
}

void CDSimController::vcardReadFailed()
{
    qWarning() << "Unable to read VCard data from SIM:" << m_phonebook.modemPath();
    updateBusy();
}

void CDSimController::readerStateChanged(QVersitReader::State state)
{
    if (state != QVersitReader::FinishedState)
        return;

    QList<QVersitDocument> results = m_contactReader.results();
    if (results.isEmpty()) {
        qDebug() << "No contacts found in SIM";
        m_simContacts.clear();
        removeAllSimContacts();
    } else {
        QVersitContactImporter importer;
        importer.importDocuments(results);
        m_simContacts = importer.contacts();
        if (m_simContacts.isEmpty()) {
            qDebug() << "No contacts imported from SIM data";
            removeAllSimContacts();
        } else {
            // import or remove contacts from local storage as necessary.
            ensureSimContactsPresent();
        }
    }

    updateBusy();
}

void CDSimController::deactivateAllSimContacts()
{
    QList<QContactId> ids = m_manager.contactIds(simSyncTargetFilter());
    if (ids.size()) {
        QList<QContact> deactivatedContacts;

        foreach (QContact contact, m_manager.contacts(ids)) {
            QContactDeactivated deactivated;
            contact.saveDetail(&deactivated);
            deactivatedContacts.append(contact);
        }

        if (!m_manager.saveContacts(&deactivatedContacts)) {
            qWarning() << "Error deactivating sim contacts";
        }
    }
}

void CDSimController::removeAllSimContacts()
{
    QList<QContactId> doomedIds = m_manager.contactIds(simSyncTargetFilter());
    if (doomedIds.size()) {
        if (!m_manager.removeContacts(doomedIds)) {
            qWarning() << "Error removing sim contacts from device storage";
        }
    }
}

void CDSimController::ensureSimContactsPresent()
{
    // Ensure all contacts from the SIM are present in the store
    QContactFetchHint hint;
    hint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactNickname::Type << QContactPhoneNumber::Type);
    hint.setOptimizationHints(QContactFetchHint::NoRelationships | QContactFetchHint::NoActionPreferences | QContactFetchHint::NoBinaryBlobs);

    QList<QContact> storedSimContacts = m_manager.contacts(simSyncTargetFilter(), QList<QContactSortOrder>(), hint);

    // Also find any deactivated SIM contacts
    storedSimContacts.append(m_manager.contacts(simSyncTargetFilter() & deactivatedFilter(), QList<QContactSortOrder>(), hint));

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

            // Convert the display label to a nickname; display label is managed by the backend
            QContactNickname nickname = simContact.detail<QContactNickname>();
            nickname.setNickname(displayLabel.label().trimmed());
            simContact.saveDetail(&nickname);

            simContact.removeDetail(&displayLabel);

            QContactSyncTarget syncTarget = simContact.detail<QContactSyncTarget>();
            syncTarget.setSyncTarget(m_simSyncTarget);
            simContact.saveDetail(&syncTarget);

            importContacts.append(simContact);
        }
    }

    if (!importContacts.isEmpty()) {
        // Import any contacts which were modified or are not currently present
        if (!m_manager.saveContacts(&importContacts)) {
            qWarning() << "Error while saving imported sim contacts";
        }
    }

    if (!existingContacts.isEmpty()) {
        // Remove any imported contacts no longer on the SIM
        QList<QContactId> obsoleteIds;
        foreach (const QContact &contact, existingContacts.values()) {
            obsoleteIds.append(contact.id());
        }

        if (!m_manager.removeContacts(obsoleteIds)) {
            qWarning() << "Error while removing obsolete sim contacts";
        }
    }
}

void CDSimController::voicemailConfigurationChanged()
{
    if (!m_voicemailConf || !m_simManager.present()) {
        // Wait until SIM is present
        return;
    }

    const QString voicemailTarget(QString::fromLatin1("voicemail"));

    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(voicemailTarget);

    QContact voicemailContact;
    foreach (const QContact &contact, m_manager.contacts(syncTargetFilter)) {
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
            if (!m_manager.removeContact(voicemailContact.id())) {
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
            nickname.setNickname(qtTrId("qtn_sim_voicemail_contact"));
            voicemailContact.saveDetail(&nickname);
        }

        QContactSyncTarget syncTarget = voicemailContact.detail<QContactSyncTarget>();
        if (syncTarget.isEmpty()) {
            syncTarget.setSyncTarget(voicemailTarget);
            voicemailContact.saveDetail(&syncTarget);
        }

        if (!m_manager.saveContact(&voicemailContact)) {
            qWarning() << "Unable to save voicemail contact";
        }
    }
}

void CDSimController::updateVoicemailConfiguration()
{
    QString variablePath(QString::fromLatin1("/sailfish/voicecall/voice_mailbox/"));
    if (m_simManager.present()) {
        variablePath.append(m_simManager.cardIdentifier());
    } else {
        variablePath.append(QString::fromLatin1("default"));
    }

    if (!m_voicemailConf || m_voicemailConf->key() != variablePath) {
        delete m_voicemailConf;
        m_voicemailConf = new MGConfItem(variablePath);
        connect(m_voicemailConf, SIGNAL(valueChanged()), this, SLOT(voicemailConfigurationChanged()));

        voicemailConfigurationChanged();
    }
}

// Instantiate the extension functions
#include <qcontactdeactivated_impl.h>
#include <qcontactstatusflags_impl.h>
