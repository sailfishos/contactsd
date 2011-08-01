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

#ifndef CDBIRTHDAYCONTROLLER_H
#define CDBIRTHDAYCONTROLLER_H

#include <QObject>
#include <QList>
#include <QtSparqlTrackerExtensions/TrackerChangeNotifier>
#include <QContact>
#include <QContactAbstractRequest>
#include <QContactManager>

class CDBirthdayCalendar;

QTM_USE_NAMESPACE

class CDBirthdayController : public QObject
{
    Q_OBJECT

public:
    explicit CDBirthdayController(QContactManager * const manager,
                                  QObject *parent = 0);
    ~CDBirthdayController();

private Q_SLOTS:
    void onGraphChanged(const QList<TrackerChangeNotifier::Quad> &deletions,
                        const QList<TrackerChangeNotifier::Quad> &insertions);
    void onFetchRequestStateChanged(const QContactAbstractRequest::State &newState);

private:
    void processNotificationQueues();
    void processNotifications(QList<TrackerChangeNotifier::Quad> &notifications,
                              QSet<QContactLocalId> &propertyChanges);
    void fetchContacts(const QList<QContactLocalId> &contactIds);
    void updateBirthdays(const QList<QContact> &changedBirthdays);

private:
    QList<TrackerChangeNotifier::Quad> mDeleteNotifications;
    QList<TrackerChangeNotifier::Quad> mInsertNotifications;
    CDBirthdayCalendar *mCalendar;
    QContactManager *mManager;
};

#endif // CDBIRTHDAYCONTROLLER_H
