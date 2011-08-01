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
#include "debug.h"

#include <ontologies.h>

#include <QContactBirthday>
#include <QContactFetchRequest>
#include <QContactLocalIdFilter>

QTM_USE_NAMESPACE

CUBI_USE_NAMESPACE_RESOURCES

using namespace Contactsd;

// The logic behind this class was strongly influenced by QctTrackerChangeListener in
// qtcontacts-tracker.
CDBirthdayController::CDBirthdayController(QContactManager * const manager,
                                           QObject *parent)
    : QObject(parent)
    , mCalendar(0)
    , mManager(manager)
{
    const QStringList contactClassIris = QStringList()
                                         << nco::PersonContact::iri()
                                         << nco::ContactGroup::iri();

    foreach (const QString &iri, contactClassIris) {
        connect(new TrackerChangeNotifier(iri, this),
            SIGNAL(changed(QList<TrackerChangeNotifier::Quad>,
                           QList<TrackerChangeNotifier::Quad>)),
            SLOT(onGraphChanged(QList<TrackerChangeNotifier::Quad>,
                                QList<TrackerChangeNotifier::Quad>)));
    }

    mCalendar = new CDBirthdayCalendar(this);

    debug() << "Created birthday controller";
}

CDBirthdayController::~CDBirthdayController()
{
}

void CDBirthdayController::onGraphChanged(const QList<TrackerChangeNotifier::Quad>& deletions,
                                          const QList<TrackerChangeNotifier::Quad>& insertions)
{
    mDeleteNotifications += deletions;
    mInsertNotifications += insertions;

    if (isDebugEnabled()) {
        TrackerChangeNotifier const * const notifier = qobject_cast<TrackerChangeNotifier *>(sender());

        if (notifier == 0) {
            warning() << Q_FUNC_INFO << "Error casting birthday change notifier";
            return;
        }

        debug() << notifier->watchedClass() << "birthday: deletions:" << deletions;
        debug() << notifier->watchedClass() << "birthday: insertions:" << insertions;
    }

    processNotificationQueues();
}

void
CDBirthdayController::onFetchRequestStateChanged(const QContactAbstractRequest::State &newState)
{
    if (newState == QContactAbstractRequest::CanceledState ||
                    QContactAbstractRequest::FinishedState) {
        debug() << "Birthday contacts fetch request finished";

        QContactFetchRequest * const fetchRequest = qobject_cast<QContactFetchRequest*>(sender());

        if (fetchRequest == 0) {
            warning() << Q_FUNC_INFO << "Invalid fetch request";
            return;
        }

        if (fetchRequest->error() != QContactManager::NoError) {
            warning() << Q_FUNC_INFO << "Error during birthday contact fetch request, code: "
                      << fetchRequest->error();
            fetchRequest->deleteLater();
            return;
        }

        updateBirthdays(fetchRequest->contacts());
        fetchRequest->deleteLater();
    }
}

void
CDBirthdayController::processNotificationQueues()
{
    QSet<QContactLocalId> birthdayChangedIds;

    // Process notification queues to determine contacts with changed birthdays.
    processNotifications(mDeleteNotifications, birthdayChangedIds);
    processNotifications(mInsertNotifications, birthdayChangedIds);

    if (isDebugEnabled()) {
        debug() << "changed birthdates: " << birthdayChangedIds.count() << birthdayChangedIds;
    }

    // Update the calendar with the birthday changes.
    if (not birthdayChangedIds.isEmpty()) {
        fetchContacts(birthdayChangedIds.toList());
    }
}

void
CDBirthdayController::processNotifications(QList<TrackerChangeNotifier::Quad> &notifications,
                                           QSet<QContactLocalId> &propertyChanges)
{
    foreach (const TrackerChangeNotifier::Quad &quad, notifications) {
        // TODO: Query the nco:birthDate ID from tracker.
        if (quad.predicate == 165) {
            propertyChanges += quad.subject;
        }
    }

    // Remove the processed notifications.
    notifications.clear();
}

void
CDBirthdayController::fetchContacts(const QList<QContactLocalId> &contactIds)
{
    QContactFetchHint fetchHint;
    static const QStringList detailDefinitions = QStringList() << QContactBirthday::DefinitionName
                                                               << QContactDisplayLabel::DefinitionName;
    fetchHint.setDetailDefinitionsHint(detailDefinitions);

    QContactLocalIdFilter fetchFilter;
    fetchFilter.setIds(contactIds);

    QContactFetchRequest * const fetchRequest = new QContactFetchRequest(this);
    fetchRequest->setManager(mManager);
    fetchRequest->setFetchHint(fetchHint);
    fetchRequest->setFilter(fetchFilter);

    connect(fetchRequest,
            SIGNAL(stateChanged(QContactAbstractRequest::State)),
            SLOT(onFetchRequestStateChanged(QContactAbstractRequest::State)));

    if (not fetchRequest->start()) {
        warning() << Q_FUNC_INFO << "Unable to start birthday contact fetch request";
        delete fetchRequest;
        return;
    }

    debug() << "Birthday contacts fetch request started";
}

void
CDBirthdayController::updateBirthdays(const QList<QContact> &changedBirthdays)
{
    foreach (const QContact &contact, changedBirthdays) {
        const QContactBirthday contactBirthday = contact.detail<QContactBirthday>();
        const QDate calendarBirthday = mCalendar->birthdayDate(contact);

        // birthDate was changed on the contact, so update the calendar.
        if (not contactBirthday.date().isNull() && contactBirthday.date() != calendarBirthday) {
            if (isDebugEnabled()) {
                debug() << "Contact with calendar birthday: " << contactBirthday.date()
                        << "changed details to: " << contact << ", so update the calendar event";
            }

            mCalendar->updateBirthday(contact);
        // birthDate was removed from the contact, so delete it from the calendar.
        } else if (contactBirthday.date().isNull() && not calendarBirthday.isNull()) {
            if (isDebugEnabled()) {
                debug() << "Contact: " << contact << " removed birthday, so delete the calendar event";
            }

            mCalendar->deleteBirthday(contact.localId());
        }
    }
}
