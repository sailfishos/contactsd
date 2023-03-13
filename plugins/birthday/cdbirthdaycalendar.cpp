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

#include <QStringBuilder>

#include <QContactName>
#include <QContactBirthday>
#include <QContactDisplayLabel>
#include <QContactNickname>
#include <QContactOrganization>
#include <QContactEmailAddress>
#include <QContactId>

#include <seasidecache.h>

#include <MLocale>

#include <KCalendarCore/RecurrenceRule>

#include "cdbirthdaycalendar.h"
#include "debug.h"

using namespace ML10N;

// A random ID.
const QLatin1String calNotebookId("b1376da7-5555-1111-2222-227549c4e570");
const QLatin1String calNotebookColor("#e00080"); // Pink
const QString calIdExtension = QLatin1String("com.nokia.birthday/");


CDBirthdayCalendar::CDBirthdayCalendar(SyncMode syncMode, QObject *parent) :
    QObject(parent),
    mCalendar(0),
    mStorage(0)
{
    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mStorage = mKCal::ExtendedCalendar::defaultStorage(mCalendar);

    MLocale * const locale = new MLocale(this);

    if (not locale->isInstalledTrCatalog(QLatin1String("calendar"))) {
        locale->installTrCatalog(QLatin1String("calendar"));
    }

    locale->connectSettings();
    connect(locale, SIGNAL(settingsChanged()), this, SLOT(onLocaleChanged()));

    MLocale::setDefault(*locale);

    mStorage->open();

    mKCal::Notebook::Ptr notebook = mStorage->notebook(calNotebookId);

    if (notebook.isNull()) {
        notebook = createNotebook();
        mStorage->addNotebook(notebook);
    } else {
        // Clear the calendar database if and only if restoring from a backup.
        switch(syncMode) {
        case KeepOldDB:
            // Force calendar name update, if a locale change happened while contactsd was not running.
            onLocaleChanged();
            break;

        case DropOldDB:
            mStorage->deleteNotebook(notebook);
            notebook = createNotebook();
            mStorage->addNotebook(notebook);
            break;
        }
    }
}

CDBirthdayCalendar::~CDBirthdayCalendar()
{
    if (mStorage) {
        mStorage->close();
    }

    qCDebug(lcContactsd) << "Destroyed birthday calendar";
}

mKCal::Notebook::Ptr CDBirthdayCalendar::createNotebook()
{
    return mKCal::Notebook::Ptr(new mKCal::Notebook(calNotebookId,
                                                    //% "Birthdays"
                                                    qtTrId("qtn_caln_birthdays"),
                                                    QLatin1String(""),
                                                    calNotebookColor,
                                                    false, // Not shared.
                                                    true, // Is master.
                                                    false, // Not synced to Ovi.
                                                    true, // Not writable.
                                                    true, // Visible.
                                                    QLatin1String("Birthday-Nokia"),
                                                    QLatin1String(""),
                                                    0));
}

QHash<QContactId, CalendarBirthday>
CDBirthdayCalendar::birthdays()
{
    if (not mStorage->loadNotebookIncidences(calNotebookId)) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Failed to load all incidences";
        return QHash<QContactId, CalendarBirthday>();
    }

    QHash<QContactId, CalendarBirthday> result;

    foreach(const KCalendarCore::Event::Ptr event, mCalendar->events()) {
        const QString eventUid = event->uid();
        const QContactId contactId = localContactId(eventUid);

        if (!contactId.isNull()) {
            result.insert(contactId, CalendarBirthday(event->dtStart().date(), event->summary()));
        } else {
            qCWarning(lcContactsd) << Q_FUNC_INFO << "Birthday event with a bad uid: " << eventUid;
        }
    }

    return result;
}

void CDBirthdayCalendar::updateBirthday(const QContact &contact)
{
    if (contact.id().isNull()) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Updating birthday for null contact";
        return;
    }

    const QDate contactBirthday = contact.detail<QContactBirthday>().date();

    if (contactBirthday.isNull()) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Contact without birthday, local ID: " << contact.id();
        return;
    }

    QString displayLabel = contact.detail<QContactDisplayLabel>().label();
    if (displayLabel.isEmpty()) {
        displayLabel = contact.detail<QContactNickname>().nickname();
    }
    if (displayLabel.isEmpty()) {
        displayLabel = contact.detail<QContactOrganization>().name();
    }
    if (displayLabel.isEmpty()) {
        displayLabel = contact.detail<QContactEmailAddress>().emailAddress();
    }

    if (displayLabel.isEmpty()) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Contact without name to use " << contact.id();
        return;
    }

    QString eventId = calendarEventId(contact.id());
    if (eventId.isEmpty()) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Failed to map contact id to calendar event id" << contact.id();
        return;
    }

    if (not mCalendar->hasValidNotebook(calNotebookId)) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Invalid notebook ID: " << calNotebookId;
        return;
    }

    // NOTE: if making changes beyond summary and date, increase event version number in controller.
    // NOTE: controller is assuming name as summary and uses that to detect changes

    // Retrieve birthday event.
    KCalendarCore::Event::Ptr event = calendarEvent(contact.id());

    if (event.isNull()) {
        // Add a new event.
        event = KCalendarCore::Event::Ptr(new KCalendarCore::Event());
        event->setUid(eventId);

        // Ensure events appear as birthdays in the calendar, NB#259710.
        event->setCategories(QStringList() << QLatin1String("BIRTHDAY"));

        if (not mCalendar->addEvent(event, calNotebookId)) {
            qCWarning(lcContactsd) << Q_FUNC_INFO << "Failed to add event to calendar";
            return;
        }
    } else {
        // Update the existing event.
        event->setReadOnly(false);
    }
    event->startUpdates();

    // Transfer birthday details from contact to calendar event.
    event->setSummary(displayLabel);

    // Event has only date information, no time.
    event->setDtStart(QDateTime(contactBirthday));
    event->setAllDay(true);

    // Must always set the recurrence as it depends on the event date.
    KCalendarCore::Recurrence *const recurrence = event->recurrence();

    if (contactBirthday.month() != 2 || contactBirthday.day() < 29) {
        // Simply setup yearly recurrence for trivial dates.
        recurrence->setStartDateTime(event->dtStart(), true);
        recurrence->setYearly(1); /* every year */
    } else {
        // For birthdays on February 29th the event shall occur on the
        // last day of February. This is February 29th in leap years,
        // and February 28th in all other years.
        //
        // NOTE: Actually this recurrence pattern will fail badly for
        // people born on February 29th of the years 2100, 2200, 2300,
        // 2500, ... - but I seriously doubt we care.
        //
        // NOTE2: Using setByYearDays() instead of just setting proper
        // start dates, since libmkcal fails to store the start dates
        // of recurrence rules.
        KCalendarCore::RecurrenceRule *rule;

        // 1. Include February 29th in leap years.
        rule = new KCalendarCore::RecurrenceRule;
        rule->setStartDt(event->dtStart());
        rule->setByYearDays(QList<int>() << 60); // Feb 29th
        rule->setRecurrenceType(KCalendarCore::RecurrenceRule::rYearly);
        rule->setFrequency(4); // every 4th year
        recurrence->addRRule(rule);

        // 2. Include February 28th starting from year after birth.
        rule = new KCalendarCore::RecurrenceRule;
        rule->setStartDt(event->dtStart());
        rule->setByYearDays(QList<int>() << 59); // Feb 28th
        rule->setRecurrenceType(KCalendarCore::RecurrenceRule::rYearly);
        rule->setFrequency(1); // every year
        recurrence->addRRule(rule);

        // 3. Exclude February 28th in leap years.
        rule = new KCalendarCore::RecurrenceRule;
        rule->setStartDt(event->dtStart());
        rule->setByYearDays(QList<int>() << 59); // Feb 28th
        rule->setRecurrenceType(KCalendarCore::RecurrenceRule::rYearly);
        rule->setFrequency(4); // every 4th year
        recurrence->addExRule(rule);

    }

    // Clear the alarms on the event
    event->clearAlarms();

    // We don't want any alarms for birthday events.
    // To set an alarm for birthday events, uncomment the code below.
    //KCalendarCore::Alarm::Ptr alarm = event->newAlarm();
    //alarm->setType(KCalendarCore::Alarm::Audio);
    //alarm->setEnabled(true);
    //alarm->setDisplayAlarm(event->summary());
    //alarm->setStartOffset(KCalendarCore::Duration(-36 * 3600 /* seconds */));

    event->setReadOnly(true);
    event->endUpdates();
    qCDebug(lcContactsd) << "Updated birthday event in calendar, local ID: " << contact.id();
}

void CDBirthdayCalendar::deleteBirthday(const QContactId &contactId)
{
    KCalendarCore::Event::Ptr event = calendarEvent(contactId);

    if (event.isNull()) {
        qCDebug(lcContactsd) << Q_FUNC_INFO << "Not found in calendar:" << contactId;
        return;
    }

    mCalendar->deleteEvent(event);

    qCDebug(lcContactsd) << "Deleted birthday event in calendar, local ID: " << event->uid();
}

void CDBirthdayCalendar::save()
{
    if (not mStorage->save()) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Failed to update birthdays in calendar";
    }
}

CalendarBirthday CDBirthdayCalendar::birthday(const QContactId &contactId)
{
    KCalendarCore::Event::Ptr event = calendarEvent(contactId);

    if (event.isNull()) {
        return CalendarBirthday();
    }

    return CalendarBirthday(event->dtStart().date(), event->summary());
}

QContactId CDBirthdayCalendar::localContactId(const QString &calendarEventId)
{
    quint32 numericId = 0;

    if (calendarEventId.startsWith(calIdExtension)) {
        numericId = calendarEventId.mid(calIdExtension.length()).toUInt();
    }

    return SeasideCache::apiId(numericId);
}

QString CDBirthdayCalendar::calendarEventId(const QContactId &contactId)
{
    quint32 id = SeasideCache::internalId(contactId);
    if (id == 0) {
        return QString();
    }
    return calIdExtension + QString::number(id);
}

KCalendarCore::Event::Ptr CDBirthdayCalendar::calendarEvent(const QContactId &contactId)
{
    const QString eventId = calendarEventId(contactId);

    if (eventId.isEmpty()) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Failed to map contact to event id" << contactId.toString();
        return KCalendarCore::Event::Ptr();
    }

    if (not mStorage->load(eventId)) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Unable to load event from calendar";
        return KCalendarCore::Event::Ptr();
    }

    KCalendarCore::Event::Ptr event = mCalendar->event(eventId);

    if (event.isNull()) {
        qCDebug(lcContactsd) << Q_FUNC_INFO << "Not found in calendar:" << contactId;
    }

    return event;
}

void CDBirthdayCalendar::onLocaleChanged()
{
    mKCal::Notebook::Ptr notebook = mStorage->notebook(calNotebookId);

    if (notebook.isNull()) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Calendar not found while changing locale";
        return;
    }

    const QString name = qtTrId("qtn_caln_birthdays");

    qCDebug(lcContactsd) << Q_FUNC_INFO << "Updating calendar name to" << name;
    notebook->setName(name);

    if (not mStorage->updateNotebook(notebook)) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "Could not save calendar";
    }
}
