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

#include <MLocale>

#include <recurrencerule.h>

#include "cdbirthdaycalendar.h"
#include "debug.h"

using namespace Contactsd;

// A random ID.
const QLatin1String calNotebookID("b1376da7-5555-1111-2222-227549c4e570");
const QLatin1String calIDExtension(" com.nokia.birthday");

CDBirthdayCalendar::CDBirthdayCalendar(QObject *parent) :
    QObject(parent),
    mCalendar(0),
    mStorage(0)
{
    mCalendar = mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mStorage = mKCal::ExtendedCalendar::defaultStorage(mCalendar);

    MLocale * const locale = new MLocale(this);

    if (not locale->isInstalledTrCatalog(QLatin1String("calendar"))) {
        locale->installTrCatalog(QLatin1String("calendar"));
    }

    locale->connectSettings();
    connect(locale, SIGNAL(settingsChanged()), this, SLOT(onLocaleChanged()));

    MLocale::setDefault(*locale);

    mStorage->open();

    if (mStorage->notebook(calNotebookID).isNull()) {
        mKCal::Notebook::Ptr nb = mKCal::Notebook::Ptr(new mKCal::Notebook(calNotebookID,
                                                                           qtTrId("qtn_caln_birthdays"),
                                                                           QLatin1String(""),
                                                                           QLatin1String("#ff0000"),
                                                                           false, // Not shared.
                                                                           true, // Is master.
                                                                           false, // Not synced to Ovi.
                                                                           false, // Writable.
                                                                           true, // Visible.
                                                                           QLatin1String("Birthday-Nokia"),
                                                                           QLatin1String(""),
                                                                           0));
        mStorage->addNotebook(nb);
    } else {
        // Force calendar name update, if a locale change happened while
        // contactsd was not running
        onLocaleChanged();
    }
}

CDBirthdayCalendar::~CDBirthdayCalendar()
{
    if (mStorage) {
        mStorage->close();
    }

    debug() << "Destroyed birthday calendar";
}

void CDBirthdayCalendar::updateBirthday(const QContact &contact)
{
    const QContactDisplayLabel displayName = contact.detail<QContactDisplayLabel>();
    const QContactBirthday cBirthday = contact.detail<QContactBirthday>();

    if (displayName.isEmpty() || cBirthday.isEmpty()) {
        warning() << Q_FUNC_INFO << "Contact without name or birthday, local ID: "
                  << contact.localId();
        return;
    }

    const QString cId = makeCalendarId(contact.localId());
    mStorage->load(cId);
    KCalCore::Event::Ptr event = mCalendar->event(cId);

    if (event.isNull()) {
        // Add a new event.
        event = KCalCore::Event::Ptr(new KCalCore::Event());
        event->startUpdates();
        event->setUid(cId);
        event->setAllDay(true);

        // Recurrence.
        KCalCore::Recurrence * const rule = event->recurrence();
        rule->setStartDateTime(event->dtStart());
        rule->setYearly(1);

        // Ensure events appear as birthdays in the calendar, NB#259710.
        event->setCategories(QStringList() << QLatin1String("BIRTHDAY"));

        if (not mCalendar->addEvent(event, calNotebookID)) {
            warning() << Q_FUNC_INFO << "Failed to add event to calendar";
            return;
        }
    } else {
        // Update the existing event.
        event->setReadOnly(false);
        event->startUpdates();
    }

    event->setSummary(displayName.label());

    event->setDtStart(KDateTime(cBirthday.date(), QTime(), KDateTime::ClockTime));
    event->setDtEnd(KDateTime(cBirthday.date().addDays(1), QTime(), KDateTime::ClockTime));

    event->setReadOnly(true);
    event->endUpdates();

    if (not mStorage->isValidNotebook(calNotebookID)) {
        warning() << Q_FUNC_INFO << "Invalid notebook ID: " << calNotebookID;
        return;
    }

    if (not mStorage->save()) {
        warning() << Q_FUNC_INFO << "Failed to save event in calendar";
        return;
    }

    debug() << "Updated birthday event in calendar, local ID: " << contact.localId();
}

QString CDBirthdayCalendar::makeCalendarId(QContactLocalId id) const
{
    return QString::number(id) + calIDExtension;
}

void CDBirthdayCalendar::deleteBirthday(QContactLocalId id)
{
    const QString cId = makeCalendarId(id);
    mStorage->load(cId);
    KCalCore::Event::Ptr event = mCalendar->event(cId);

    if (event.isNull()) {
        warning() << Q_FUNC_INFO << "Not found in calendar";
        return;
    }

    mCalendar->deleteEvent(event);

    if (not mStorage->save()) {
        warning() << Q_FUNC_INFO << "Failed to delete event from calendar";
        return;
    }

    debug() << "Deleted birthday event in calendar, local ID: " << cId;
}

QDate CDBirthdayCalendar::birthdayDate(const QContact &contact)
{
    const QString cId = makeCalendarId(contact.localId());

    if (not mStorage->load(cId)) {
        warning() << Q_FUNC_INFO << "Unable to load event from calendar";
        return QDate();
    }

    KCalCore::Event::Ptr event = mCalendar->event(cId);

    if (event.isNull()) {
        warning() << Q_FUNC_INFO << "Not found in calendar";
        return QDate();
    }

    return event->dtStart().date();
}

void CDBirthdayCalendar::onLocaleChanged()
{
    mKCal::Notebook::Ptr notebook = mStorage->notebook(calNotebookID);

    if (notebook == 0) {
        warning() << Q_FUNC_INFO << "Calendar not found while changing locale";
        return;
    }

    const QString name = qtTrId("qtn_caln_birthdays");

    debug() << Q_FUNC_INFO << "Updating calendar name to" << name;
    notebook->setName(name);

    if (not mStorage->updateNotebook(notebook)) {
        warning() << Q_FUNC_INFO << "Could not save calendar";
    }
}
