/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2014 Jolla Ltd.
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

#include "cdexportercontroller.h"
#include "cdexporterplugin.h"
#include "debug.h"

#include <qtcontacts-extensions_impl.h>
#include <qtcontacts-extensions_manager_impl.h>

#include <contactmanagerengine.h>
#include <twowaycontactsyncadapter.h>
#include <twowaycontactsyncadapter_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include <QContactChangeLogFilter>
#include <QContactGuid>

#include <QDateTime>

#include <unistd.h>

using namespace Contactsd;

namespace {

const QString exportSyncTarget(QStringLiteral("export"));
const QString aggregateSyncTarget(QStringLiteral("aggregate"));
const QString oobIdsKey(QStringLiteral("privilegedIds"));
const QString avatarPathsKey(QStringLiteral("avatarPaths"));

// Delay 500ms for accumulate futher changes when a contact is updated
// Wait 10s for further changes when a contact presence is updated
const int syncDelay = 500;
const int presenceSyncDelay = 10000;

QMap<QString, QString> privilegedManagerParameters()
{
    QMap<QString, QString> rv;
    rv.insert(QStringLiteral("mergePresenceChanges"), QStringLiteral("false"));
    return rv;
}

QMap<QString, QString> nonprivilegedManagerParameters()
{
    QMap<QString, QString> rv;
    rv.insert(QStringLiteral("mergePresenceChanges"), QStringLiteral("false"));
    rv.insert(QStringLiteral("nonprivileged"), QStringLiteral("true"));
    return rv;
}

QString managerName()
{
    return QStringLiteral("org.nemomobile.contacts.sqlite");
}

QContactDetailFilter syncTargetFilter(const QString &syncTarget)
{
    QContactDetailFilter filter;
    filter.setDetailType(QContactSyncTarget::Type, QContactSyncTarget::FieldSyncTarget);
    filter.setValue(syncTarget);
    return filter;
}

QContactFilter modifiedSinceFilter(const QDateTime &ts)
{
    // Return local contacts modified since the timestamp
    QContactChangeLogFilter addedFilter, changedFilter;

    addedFilter.setEventType(QContactChangeLogFilter::EventAdded);
    addedFilter.setSince(ts);

    changedFilter.setEventType(QContactChangeLogFilter::EventChanged);
    changedFilter.setSince(ts);

    return syncTargetFilter(aggregateSyncTarget) & (addedFilter | changedFilter);
}

QContactFilter removedSinceFilter(const QDateTime &ts)
{
    // Return local contacts removed since the timestamp
    QContactChangeLogFilter removedFilter;

    removedFilter.setEventType(QContactChangeLogFilter::EventRemoved);
    removedFilter.setSince(ts);

    return syncTargetFilter(aggregateSyncTarget) & removedFilter;
}

void removeProvenanceInformation(QContact &contact)
{
    foreach (const QContactDetail &detail, contact.details()) {
        if (detail.hasValue(QContactDetail__FieldProvenance)) {
            QContactDetail copy(detail);
            copy.removeValue(QContactDetail__FieldProvenance);
            contact.saveDetail(&copy);
        }
    }
}

QHash<QUrl, QUrl> modifyAvatarUrls(QContact &contact)
{
    QHash<QUrl, QUrl> changes;

    foreach (const QContactAvatar &avatar, contact.details<QContactAvatar>()) {
        const QUrl imageUrl(avatar.imageUrl());
        if (imageUrl.scheme().isEmpty() || imageUrl.isLocalFile()) {
            // Avatar paths may indicate files not accessible to non-privileged apps
            // Link to them from an accessible path, and update the stored path in the avatar detail
            const QString path(avatar.imageUrl().path());
            if (QFile::exists(path)) {
                const QFileInfo fi(path);
                const QString privilegedPath(fi.absoluteFilePath());

                int index = privilegedPath.indexOf(QStringLiteral("/privileged/Contacts/"));
                if (index != -1) {
                    // Try to make a nonprivileged version of this file
                    QString nonprivilegedPath(QString(privilegedPath).remove(index, 11));
                    if (!QFile::exists(nonprivilegedPath)) {
                        // Ensure the target directory exists
                        index = nonprivilegedPath.lastIndexOf(QLatin1Char('/'));
                        if (index) {
                            const QString dirPath(nonprivilegedPath.left(index));
                            QDir dir(dirPath);
                            if (!dir.exists() && !QDir::root().mkpath(dirPath)) {
                                qWarning() << "Unable to create directory path:" << dirPath;
                            }
                        }

                        // Try to link the file to the new path
                        QByteArray oldPath(privilegedPath.toUtf8());
                        QByteArray newPath(nonprivilegedPath.toUtf8());
                        if (::link(oldPath.constData(), newPath.constData()) != 0) {
                            qWarning() << "Unable to create link to:" << privilegedPath << nonprivilegedPath;
                            nonprivilegedPath = QString();
                        }
                    }

                    // Update the avatar to point to the alternative path
                    QContactAvatar copy(avatar);
                    copy.setImageUrl(QUrl::fromLocalFile(nonprivilegedPath));
                    contact.saveDetail(&copy);

                    changes.insert(copy.imageUrl(), avatar.imageUrl());
                }
            }
        }
    }

    return changes;
}

void reverseAvatarChanges(QContact &contact, const QHash<QUrl, QUrl> &changes)
{
    foreach (const QContactAvatar &avatar, contact.details<QContactAvatar>()) {
        const QHash<QUrl, QUrl>::const_iterator it = changes.constFind(avatar.imageUrl());
        if (it != changes.constEnd()) {
            // This avatar's URL is one we changed it to; revert the change to prevent
            // it being detected as a nonprivileged modification
            QContactAvatar copy(avatar);
            copy.setImageUrl(*it);
            contact.saveDetail(&copy);
        }
    }
}

QSet<QContactDetail::DetailType> getIgnorableDetailTypes()
{
    QSet<QContactDetail::DetailType> rv;
    rv.insert(QContactDetail__TypeDeactivated);
    rv.insert(QContactDetail::TypeDisplayLabel);
    rv.insert(QContactDetail__TypeIncidental);
    rv.insert(QContactDetail__TypeStatusFlags);
    rv.insert(QContactDetail::TypeSyncTarget);
    rv.insert(QContactDetail::TypeTimestamp);
    return rv;
}

const QSet<QContactDetail::DetailType> &ignorableDetailTypes()
{
    static QSet<QContactDetail::DetailType> types(getIgnorableDetailTypes());
    return types;
}

class SyncAdapter : public QtContactsSqliteExtensions::TwoWayContactSyncAdapter
{
private:
    QString m_accountId;
    QContactManager &m_privileged;
    QContactManager &m_nonprivileged;
    QDateTime m_remoteSince;
    QMap<QContactId, QContactId> m_privilegedIds;
    QMap<QContactId, QContactId> m_nonprivilegedIds;
    QContactId m_privilegedSelfId;
    QContactId m_nonprivilegedSelfId;
    QHash<QContactId, QHash<QUrl, QUrl> > m_avatarPathChanges;

    bool prepareSync()
    {
        if (!initSyncAdapter(m_accountId)) {
            qWarning() << "Unable to initialize sync adapter";
            return false;
        }

        if (!readSyncStateData(&m_remoteSince, m_accountId)) {
            qWarning() << "Unable to read sync state data";
            return false;
        }

        // Read our extra OOB data
        QMap<QString, QVariant> values;
        if (!d->m_engine->fetchOOB(d->m_stateData[m_accountId].m_oobScope, QStringList() << oobIdsKey << avatarPathsKey, &values)) {
            qWarning() << "Failed to read sync state data for" << exportSyncTarget;
            return false;
        }

        QMap<QString, QString> privilegedIds;
        {
            QByteArray cdata = values.value(oobIdsKey).toByteArray();
            QDataStream ds(cdata);
            ds >> privilegedIds;
        }

        QMap<QString, QString>::const_iterator it = privilegedIds.constBegin(), end = privilegedIds.constEnd();
        for ( ; it != end; ++it) {
            const QContactId nonprivilegedId(QContactId::fromString(it.key()));
            const QContactId privilegedId(QContactId::fromString(it.value()));

            m_privilegedIds.insert(nonprivilegedId, privilegedId);
            m_nonprivilegedIds.insert(privilegedId, nonprivilegedId);
        }

        // Ensure that the self IDS are mapped to each other
        if (m_privilegedIds.isEmpty()) {
            m_privilegedIds.insert(m_nonprivilegedSelfId, m_privilegedSelfId);
            m_nonprivilegedIds.insert(m_privilegedSelfId, m_nonprivilegedSelfId);
        }

        // Retrieve any avatar path changes we have made
        {
            QByteArray cdata = values.value(avatarPathsKey).toByteArray();
            QDataStream ds(cdata);
            ds >> m_avatarPathChanges;
        }

        return true;
    }

    bool finalizeSync()
    {
        // Store the set of existing IDs to OOB
        QMap<QString, QVariant> values;

        QByteArray cdata;
        {
            QMap<QString, QString> privilegedIds;
            QMap<QContactId, QContactId>::const_iterator it = m_privilegedIds.constBegin(), end = m_privilegedIds.constEnd();
            for ( ; it != end; ++it) {
                privilegedIds.insert(it.key().toString(), it.value().toString());
            }

            QDataStream write(&cdata, QIODevice::WriteOnly);
            write << privilegedIds;
        }
        values.insert(oobIdsKey, QVariant(cdata));

        cdata.clear();
        {
            QDataStream write(&cdata, QIODevice::WriteOnly);
            write << m_avatarPathChanges;
        }
        values.insert(avatarPathsKey, QVariant(cdata));

        if (!d->m_engine->storeOOB(d->m_stateData[m_accountId].m_oobScope, values)) {
            qWarning() << "Failed to store sync state data to OOB storage";
            return false;
        }

        if (!storeSyncStateData(m_accountId)) {
            qWarning() << "Unable to store final state after sync completion";
            return false;
        }

        return true;
    }

    void prepareImportContact(QContact &contact, const QContactId &privilegedId)
    {
        // Remove the timestamp detail from this contact
        QContactTimestamp ts(contact.detail<QContactTimestamp>());
        contact.removeDetail(&ts);

        // Remap avatar path changes
        if (!privilegedId.isNull()) {
            // If we modified this contact's avatar path, reverse that change
            QHash<QContactId, QHash<QUrl, QUrl> >::const_iterator it = m_avatarPathChanges.constFind(privilegedId);
            if (it != m_avatarPathChanges.constEnd()) {
                reverseAvatarChanges(contact, *it);
            }
        }

        removeProvenanceInformation(contact);
    }

    bool syncNonprivilegedToPrivileged()
    {
        QList<QContact> modifiedContacts;
        QList<QContact> removedContacts;
        QContact selfContact;

        QList<QPair<QContactId, int> > additionIds;

        // Find nonprivileged changes since our last sync
        QList<QContactId> modifiedIds = m_nonprivileged.contactIds(modifiedSinceFilter(m_remoteSince));
        QList<QContactId> removedIds = m_nonprivileged.contactIds(removedSinceFilter(m_remoteSince));

        if (!modifiedIds.isEmpty() || !removedIds.isEmpty()) {
            foreach (QContact contact, m_nonprivileged.contacts(modifiedIds)) {
                // Find the primary DB ID for this contact, if it exists there
                const QContactId nonprivilegedId(contact.id());

                // Self contact must be treated separately
                if (nonprivilegedId == m_nonprivilegedSelfId) {
                    selfContact = contact;
                    selfContact.setId(m_privilegedSelfId);

                    prepareImportContact(selfContact, m_privilegedSelfId);
                    continue;
                }

                const QContactId privilegedId(m_privilegedIds.value(nonprivilegedId));
                contact.setId(privilegedId);
                if (privilegedId.isNull()) {
                    // This is an addition
                    additionIds.append(qMakePair(nonprivilegedId, modifiedContacts.count()));
                }

                // Reset the syncTarget to 'aggregate' for this contact
                QContactSyncTarget st(contact.detail<QContactSyncTarget>());
                st.setSyncTarget(aggregateSyncTarget);
                contact.saveDetail(&st);

                // Remove any GUID from this contact
                QContactGuid guid(contact.detail<QContactGuid>());
                contact.removeDetail(&guid);

                prepareImportContact(contact, privilegedId);

                modifiedContacts.append(contact);
            }

            foreach (const QContactId &nonprivilegedId, removedIds) {
                const QContactId privilegedId(m_privilegedIds.value(nonprivilegedId));
                if (!privilegedId.isNull()) {
                    QContact contact;
                    contact.setId(privilegedId);
                    removedContacts.append(contact);
                } else {
                    qWarning() << "Cannot remove export deletion without primary ID:" << nonprivilegedId;
                }
            }
        }

qDebug() << "remote changes ================================";
qDebug() << "m_remoteSince:" << m_remoteSince;
qDebug() << "removedContacts:" << removedContacts;
qDebug() << "modifiedContacts:" << modifiedContacts;

        if (!storeRemoteChanges(removedContacts, &modifiedContacts, m_accountId)) {
            qWarning() << "Unable to store remote changes";
            return false;
        }

        if (!additionIds.isEmpty()) {
            // Find the IDs allocated in the primary DB
            QList<QPair<QContactId, int> >::const_iterator it = additionIds.constBegin(), end = additionIds.constEnd();
            for ( ; it != end; ++it) {
                const QContactId &nonprivilegedId((*it).first);
                const int additionIndex((*it).second);
                const QContactId privilegedId(modifiedContacts.at(additionIndex).id());
                m_privilegedIds.insert(nonprivilegedId, privilegedId);
                m_nonprivilegedIds.insert(privilegedId, nonprivilegedId);
            }
        }

        if (!selfContact.id().isNull()) {
            // Store the self contact changes separately, with the original sync target
            removedContacts.clear();
            modifiedContacts.clear();
            modifiedContacts.append(selfContact);
            if (!storeRemoteChanges(removedContacts, &modifiedContacts, m_accountId)) {
                qWarning() << "Unable to store remote changes to self contact";
                return false;
            }
        }

        return true;
    }

    void prepareExportContact(QContact &contact, const QContactId &privilegedId)
    {
        // Remove the timestamp detail
        QContactTimestamp ts(contact.detail<QContactTimestamp>());
        contact.removeDetail(&ts);

        // Remap avatar path changes
        QHash<QUrl, QUrl> changes = modifyAvatarUrls(contact);
        m_avatarPathChanges.insert(privilegedId, changes);

        removeProvenanceInformation(contact);
    }

    bool syncPrivilegedToNonprivileged()
    {
        // Find privileged DB changes we need to reflect (including presence changes)
        QDateTime localSince;
        QList<QContact> locallyAdded, locallyModified, locallyDeleted;
        if (!determineLocalChanges(&localSince, &locallyAdded, &locallyModified, &locallyDeleted, m_accountId, ignorableDetailTypes())) {
            qWarning() << "Unable to determine local changes";
            return false;
        }

qDebug() << "local changes --------------------------------";
qDebug() << "localSince:" << localSince;
qDebug() << "locallyAdded:" << locallyAdded;
qDebug() << "locallyModified:" << locallyModified;
qDebug() << "locallyDeleted:" << locallyDeleted;

        QList<QContact> modifiedContacts;
        QList<QContactId> removedContactIds;
        QContact selfContact;

        QList<QPair<QContactId, int> > additionIds;

        // Apply primary DB changes to the nonprivileged DB
        foreach (QContact contact, locallyAdded + locallyModified) {
            const QContactId privilegedId(contact.id());

            if (privilegedId == m_privilegedSelfId) {
                selfContact = contact;
                selfContact.setId(m_nonprivilegedSelfId);

                prepareExportContact(selfContact, m_privilegedSelfId);
                continue;
            }

            const QContactId nonprivilegedId(m_nonprivilegedIds.value(privilegedId));
            contact.setId(nonprivilegedId);
            if (nonprivilegedId.isNull()) {
                // This is an addition
                additionIds.append(qMakePair(privilegedId, modifiedContacts.count()));

                // Remove the primary DB ID
                contact.setId(QContactId());
            }

            // Represent this contact as an aggregate in the export DB
            QContactSyncTarget st = contact.detail<QContactSyncTarget>();
            st.setSyncTarget(aggregateSyncTarget);
            contact.saveDetail(&st);

            prepareExportContact(contact, privilegedId);

            modifiedContacts.append(contact);
        }

        foreach (QContact contact, locallyDeleted) {
            const QContactId privilegedId(contact.id());
            const QContactId nonprivilegedId(m_nonprivilegedIds.value(privilegedId));
            if (!nonprivilegedId.isNull()) {
                removedContactIds.append(nonprivilegedId);
            }

            m_avatarPathChanges.remove(privilegedId);
        }

        if (!modifiedContacts.isEmpty()) {
            if (!m_nonprivileged.saveContacts(&modifiedContacts)) {
                qWarning() << "Unable to save privileged DB changes to export DB!";
                return false;
            }

            if (!additionIds.isEmpty()) {
                // Find the IDs allocated in the export DB
                QList<QPair<QContactId, int> >::const_iterator it = additionIds.constBegin(), end = additionIds.constEnd();
                for ( ; it != end; ++it) {
                    const QContactId &privilegedId((*it).first);
                    const int additionIndex((*it).second);
                    const QContactId nonprivilegedId(modifiedContacts.at(additionIndex).id());
                    m_nonprivilegedIds.insert(privilegedId, nonprivilegedId);
                    m_privilegedIds.insert(nonprivilegedId, privilegedId);
                }
            }
        }

        if (!removedContactIds.isEmpty()) {
            if (!m_nonprivileged.removeContacts(removedContactIds)) {
                qWarning() << "Unable to remove privileged DB deletions from export DB!";
                return false;
            }
        }

        if (!selfContact.id().isNull()) {
            if (!m_nonprivileged.saveContact(&selfContact)) {
                qWarning() << "Unable to save privileged DB self contact changes to export DB!";
                return false;
            }
        }

        return true;
    }

public:
    SyncAdapter(QContactManager &privileged, QContactManager &nonprivileged)
        : TwoWayContactSyncAdapter(exportSyncTarget, privileged)
        , m_privileged(privileged)
        , m_nonprivileged(nonprivileged)
    {
        m_privilegedSelfId = m_privileged.selfContactId();
        m_nonprivilegedSelfId = m_nonprivileged.selfContactId();
    }

    bool sync()
    {
        // Proceed through the steps of the TWCSA algorithm, where the nonprivileged database
        // is equivalent to 'remote' and the privileged datsbase is euqivalent to 'local'
        return (prepareSync() &&
                syncNonprivilegedToPrivileged() &&
                syncPrivilegedToNonprivileged() &&
                finalizeSync());
    }
};

}

CDExporterController::CDExporterController(QObject *parent)
    : QObject(parent)
    , m_privilegedManager(managerName(), privilegedManagerParameters())
    , m_nonprivilegedManager(managerName(), nonprivilegedManagerParameters())
{
    // Use a timer to delay reaction, so we don't sync until sequential changes have completed
    m_syncTimer.setSingleShot(true);
    connect(&m_syncTimer, SIGNAL(timeout()), this, SLOT(onSyncTimeout()));

    connect(&m_privilegedManager, SIGNAL(contactsAdded(QList<QContactId>)), this, SLOT(onPrivilegedContactsAdded(QList<QContactId>)));
    connect(&m_privilegedManager, SIGNAL(contactsChanged(QList<QContactId>)), this, SLOT(onPrivilegedContactsChanged(QList<QContactId>)));
    connect(&m_privilegedManager, SIGNAL(contactsRemoved(QList<QContactId>)), this, SLOT(onPrivilegedContactsRemoved(QList<QContactId>)));

    // Presence changes are reported by the engine
    QtContactsSqliteExtensions::ContactManagerEngine *engine(QtContactsSqliteExtensions::contactManagerEngine(m_privilegedManager));
    connect(engine, SIGNAL(contactsPresenceChanged(QList<QContactId>)), this, SLOT(onPrivilegedContactsPresenceChanged(QList<QContactId>)));

    connect(&m_nonprivilegedManager, SIGNAL(contactsAdded(QList<QContactId>)), this, SLOT(onNonprivilegedContactsAdded(QList<QContactId>)));
    connect(&m_nonprivilegedManager, SIGNAL(contactsChanged(QList<QContactId>)), this, SLOT(onNonprivilegedContactsChanged(QList<QContactId>)));
    connect(&m_nonprivilegedManager, SIGNAL(contactsRemoved(QList<QContactId>)), this, SLOT(onNonprivilegedContactsRemoved(QList<QContactId>)));

    // Do an initial sync
    m_syncTimer.start(1);
}

CDExporterController::~CDExporterController()
{
}

void CDExporterController::onPrivilegedContactsAdded(const QList<QContactId> &addedIds)
{
    Q_UNUSED(addedIds)
    scheduleSync(DataChange);
}

void CDExporterController::onPrivilegedContactsChanged(const QList<QContactId> &changedIds)
{
    Q_UNUSED(changedIds)
    scheduleSync(DataChange);
}

void CDExporterController::onPrivilegedContactsPresenceChanged(const QList<QContactId> &changedIds)
{
    Q_UNUSED(changedIds)
    scheduleSync(PresenceChange);
}

void CDExporterController::onPrivilegedContactsRemoved(const QList<QContactId> &removedIds)
{
    Q_UNUSED(removedIds)
    scheduleSync(DataChange);
}

void CDExporterController::onNonprivilegedContactsAdded(const QList<QContactId> &addedIds)
{
    Q_UNUSED(addedIds)
    scheduleSync(DataChange);
}

void CDExporterController::onNonprivilegedContactsChanged(const QList<QContactId> &changedIds)
{
    Q_UNUSED(changedIds)
    scheduleSync(DataChange);
}

void CDExporterController::onNonprivilegedContactsRemoved(const QList<QContactId> &removedIds)
{
    Q_UNUSED(removedIds)
    scheduleSync(DataChange);
}

void CDExporterController::onSyncTimeout()
{
    // Perform a sync
    SyncAdapter adapter(m_privilegedManager, m_nonprivilegedManager);
    if (!adapter.sync()) {
        qWarning() << "Unable to synchronize database changes!";
    }
}

void CDExporterController::scheduleSync(ChangeType type)
{
    // Something has changed that needs to be exported
    m_syncTimer.start(type == PresenceChange ? presenceSyncDelay : syncDelay);
}

