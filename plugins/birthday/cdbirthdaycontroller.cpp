/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "cdbirthdaycontroller.h"
#include "cdbirthdaycalendar.h"
#include "cdbirthdayplugin.h"
#include "debug.h"

#include <QDir>
#include <QFile>

#include <QContactBirthday>
#include <QContactDetailFilter>
#include <QContactFetchRequest>
#include <QContactSyncTarget>
#include <QContactIdFilter>
#include <QContactDisplayLabel>

QTCONTACTS_USE_NAMESPACE

using namespace Contactsd;

namespace {

// version number for current type of birthday events. Increase number when changing event details.
const int CURRENT_BIRTHDAY_VERSION = 1;
const int UPDATE_TIMEOUT = 1000; // ms

template<typename DetailType>
QContactDetailFilter detailFilter(int field = -1)
{
    QContactDetailFilter filter;
    filter.setDetailType(DetailType::Type, field);
    return filter;
}

QMap<QString, QString> managerParameters()
{
    // We don't need to handle presence changes, so report them separately and ignore them
    QMap<QString, QString> parameters;
    parameters.insert(QString::fromLatin1("mergePresenceChanges"), QString::fromLatin1("false"));
    return parameters;
}

}

CDBirthdayController::CDBirthdayController(QObject *parent)
    : QObject(parent)
    , mCalendar(stampFileUpToDate() ? CDBirthdayCalendar::KeepOldDB : CDBirthdayCalendar::DropOldDB)
    , mManager(QStringLiteral("org.nemomobile.contacts.sqlite"), managerParameters())
    , mRequest(new QContactFetchRequest)
    , mSyncMode(Incremental)
    , mUpdateAllPending(false)
{
    connect(&mManager, SIGNAL(contactsAdded(QList<QContactId>)),
            SLOT(contactsChanged(QList<QContactId>)));
    connect(&mManager, SIGNAL(contactsChanged(QList<QContactId>)),
            SLOT(contactsChanged(QList<QContactId>)));
    connect(&mManager, SIGNAL(contactsRemoved(QList<QContactId>)),
            SLOT(contactsRemoved(QList<QContactId>)));

    connect(&mManager, SIGNAL(dataChanged()), SLOT(updateAllBirthdays()));

    updateAllBirthdays();

    mUpdateTimer.setInterval(UPDATE_TIMEOUT);
    mUpdateTimer.setSingleShot(true);
    connect(&mUpdateTimer, SIGNAL(timeout()), SLOT(onUpdateQueueTimeout()));
}

CDBirthdayController::~CDBirthdayController()
{
}

void
CDBirthdayController::contactsChanged(const QList<QContactId>& contacts)
{
    foreach (const QContactId &id, contacts)
        mUpdatedContacts.insert(id);

    // Just restart the timer - if it doesn't expire, we can afford to wait
    mUpdateTimer.start();
}

void CDBirthdayController::contactsRemoved(const QList<QContactId>& contacts)
{
    foreach (const QContactId &id, contacts)
        mCalendar.deleteBirthday(id);
    mCalendar.save();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Full sync logic
///////////////////////////////////////////////////////////////////////////////////////////////////

bool
CDBirthdayController::stampFileUpToDate()
{
    QFile cacheFile(stampFilePath());
    if (!cacheFile.exists()) {
        return false;
    }

    if (!cacheFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray content = cacheFile.read(100);
    bool ok;
    int value = content.toInt(&ok);
    return (ok && value == CURRENT_BIRTHDAY_VERSION);
}

void
CDBirthdayController::createStampFile()
{
    QFile cacheFile(stampFilePath());

    if (not cacheFile.open(QIODevice::WriteOnly)) {
        warning() << Q_FUNC_INFO << "Unable to create birthday plugin stamp file "
                  << cacheFile.fileName() << " with error " << cacheFile.errorString();
    }

    cacheFile.write(QByteArray::number(CURRENT_BIRTHDAY_VERSION));
}

QString
CDBirthdayController::stampFilePath()
{
    return BasePlugin::cacheFileName(QLatin1String("calendar.stamp"));
}

void
CDBirthdayController::updateAllBirthdays()
{
    if (mRequest->isActive()) {
        mUpdateAllPending = true;
    } else {
        // Fetch every contact with a birthday.
        fetchContacts(detailFilter<QContactBirthday>(), FullSync);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Incremental sync logic
///////////////////////////////////////////////////////////////////////////////////////////////////

void
CDBirthdayController::onUpdateQueueTimeout()
{
    if (mRequest->isActive()) {
        // The timer will be restarted by completion of the active request
        return;
    }

    QList<QContactId> contactIds(mUpdatedContacts.toList());

    // If we request too many contact IDs, we will exceed the SQLite bound variable limit
    const int batchSize = 200;
    if (contactIds.count() > batchSize) {
        mUpdatedContacts = contactIds.mid(batchSize).toSet();
        contactIds = contactIds.mid(0, batchSize);
    } else {
        mUpdatedContacts.clear();
    }

    QContactIdFilter fetchFilter;
    fetchFilter.setIds(contactIds);

    fetchContacts(fetchFilter, Incremental);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Common sync logic
///////////////////////////////////////////////////////////////////////////////////////////////////

void
CDBirthdayController::fetchContacts(const QContactFilter &filter, SyncMode mode)
{
    // Set up the fetch request object
    mRequest->setManager(&mManager);

    QContactFetchHint fetchHint;
    fetchHint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactBirthday::Type << QContactDisplayLabel::Type);
    fetchHint.setOptimizationHints(QContactFetchHint::NoRelationships |
                                   QContactFetchHint::NoActionPreferences |
                                   QContactFetchHint::NoBinaryBlobs);
    mRequest->setFetchHint(fetchHint);

    // Only fetch aggregate contacts
    QContactDetailFilter syncTargetFilter(detailFilter<QContactSyncTarget>(QContactSyncTarget::FieldSyncTarget));
    syncTargetFilter.setValue(QStringLiteral("aggregate"));
    mRequest->setFilter(filter & syncTargetFilter);

    connect(mRequest.data(), SIGNAL(stateChanged(QContactAbstractRequest::State)),
            SLOT(onRequestStateChanged(QContactAbstractRequest::State)));

    if (!mRequest->start()) {
        warning() << Q_FUNC_INFO << "Unable to start birthday contact fetch request";
    } else {
        debug() << "Birthday contacts fetch request started";
        mSyncMode = mode;
    }
}

void
CDBirthdayController::onRequestStateChanged(QContactAbstractRequest::State newState)
{
    if (newState == QContactAbstractRequest::FinishedState) {
        debug() << "Birthday contacts fetch request finished";

        if (mRequest->error() != QContactManager::NoError) {
            warning() << Q_FUNC_INFO << "Error during birthday contact fetch request, code:" << mRequest->error();
        } else {
            if (mSyncMode == FullSync) {
                syncBirthdays(mRequest->contacts());

                // Create the stamp file only after a successful full sync.
                createStampFile();
            } else {
                updateBirthdays(mRequest->contacts());
            }
        }

        // We're finished with this request, clear it out to drop any contact data
        // (although don't delete it directly, as we're currently handling a signal from it)
        mRequest.take()->deleteLater();
        mRequest.reset(new QContactFetchRequest);
    } else if (newState == QContactAbstractRequest::CanceledState) {
        debug() << "Birthday contacts fetch request canceled";
    } else {
        // Request still in progress
        return;
    }

    // Save the calendar in any case (success or not), since this "save" call
    // also applies for the deleteBirthday() calls in processNotificationQueues()
    mCalendar.save();

    if (mUpdateAllPending) {
        // We need to update all birthdays
        mUpdateAllPending = false;
        updateAllBirthdays();
    } else if (!mUpdatedContacts.isEmpty() && !mUpdateTimer.isActive()) {
        // If some updated contacts weren't requested, we need to go again
        mUpdateTimer.start();
    }
}

void
CDBirthdayController::updateBirthdays(const QList<QContact> &changedBirthdays)
{
    foreach (const QContact &contact, changedBirthdays) {
        const QContactBirthday contactBirthday = contact.detail<QContactBirthday>();
        const QString contactDisplayLabel = contact.detail<QContactDisplayLabel>().label();
        const CalendarBirthday calendarBirthday = mCalendar.birthday(contact.id());

        // Display label or birthdate was removed from the contact, so delete it from the calendar.
        if (contactDisplayLabel.isEmpty() || contactBirthday.date().isNull()) {
            if (!calendarBirthday.date().isNull()) {
                debug() << "Contact: " << contact.id() << " removed birthday or displayLabel, so delete the calendar event";

                mCalendar.deleteBirthday(contact.id());
            }
        // Display label or birthdate was changed on the contact, so update the calendar.
        } else if ((contactDisplayLabel != calendarBirthday.summary()) ||
                   (contactBirthday.date() != calendarBirthday.date())) {
            debug() << "Contact with calendar birthday: " << calendarBirthday.date()
                    << " and calendar displayLabel: " << calendarBirthday.summary()
                    << " changed details to: " << contactBirthday.date() << contactDisplayLabel
                    << ", so update the calendar event";

            mCalendar.updateBirthday(contact);
        }
    }
}

void
CDBirthdayController::syncBirthdays(const QList<QContact> &birthdayContacts)
{
    QHash<QContactId, CalendarBirthday> oldBirthdays = mCalendar.birthdays();

    // Check all birthdays from the contacts if the stored calendar item is up-to-date
    foreach (const QContact &contact, birthdayContacts) {
        const QString contactDisplayLabel = contact.detail<QContactDisplayLabel>().label();
        if (contactDisplayLabel.isEmpty()) {
            debug() << "Contact: " << contact << " has no displayLabel, so not syncing to calendar";
            continue;
        }

        QHash<QContactId, CalendarBirthday>::Iterator it = oldBirthdays.find(contact.id());

        if (oldBirthdays.end() != it) {
            const QContactBirthday contactBirthday = contact.detail<QContactBirthday>();
            const CalendarBirthday &calendarBirthday = *it;

            // Display label or birthdate was changed on the contact, so update the calendar.
            if ((contactDisplayLabel != calendarBirthday.summary()) ||
                (contactBirthday.date() != calendarBirthday.date())) {
                debug() << "Contact with calendar birthday: " << contactBirthday.date()
                        << " and calendar displayLabel: " << calendarBirthday.summary()
                        << " changed details to: " << contact << ", so update the calendar event";

                mCalendar.updateBirthday(contact);
            }

            // Birthday exists, so not a garbage one
            oldBirthdays.erase(it);
        } else {
            // Create new birthday
            mCalendar.updateBirthday(contact);
        }
    }

    // Remaining old birthdays in the calendar db do not did not match any contact, so remove them.
    foreach (const QContactId &id, oldBirthdays.keys()) {
        debug() << "Birthday with contact id" << id << "no longer has a matching contact, trashing it";
        mCalendar.deleteBirthday(id);
    }
}
