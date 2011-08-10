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
#include "debug.h"

#include <ontologies.h>

CUBI_USE_NAMESPACE_RESOURCES

using namespace Contactsd;

// The logic behind this class was strongly influenced by QctTrackerChangeListener in
// qtcontacts-tracker.
CDBirthdayController::CDBirthdayController(QObject *parent) : QObject(parent)
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
CDBirthdayController::processNotificationQueues()
{
    QSet<QContactLocalId> birthdayChangedIds;

    processNotifications(mDeleteNotifications, birthdayChangedIds);
    processNotifications(mInsertNotifications, birthdayChangedIds);

    if (isDebugEnabled()) {
        debug() << "changed birthdates: " << birthdayChangedIds.count() << birthdayChangedIds;
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
