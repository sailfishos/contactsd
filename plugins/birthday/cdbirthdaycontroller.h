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

#include <QtCore>

#include <QContact>
#include <QContactAbstractRequest>
#include <QContactFetchRequest>
#include <QContactManager>

#include <QtSparql>

#include <QtSparqlTrackerExtensions/TrackerChangeNotifier>

class CDBirthdayCalendar;

QTM_USE_NAMESPACE

class CDBirthdayController : public QObject
{
    Q_OBJECT

    enum SyncMode {
        Incremental,
        FullSync
    };

public:
    explicit CDBirthdayController(QSparqlConnection &connection,
                                  QObject *parent = 0);
    ~CDBirthdayController();

private Q_SLOTS:
    void onTrackerIdsFetched();
    void onGraphChanged(const QList<TrackerChangeNotifier::Quad> &deletions,
                        const QList<TrackerChangeNotifier::Quad> &insertions);
    void onFetchRequestStateChanged(QContactAbstractRequest::State newState);
    void onFullSyncRequestStateChanged(QContactAbstractRequest::State newState);

private:
    void fetchTrackerIds();
    bool stampFileExists();
    void createStampFile();
    QString stampFilePath() const;
    void updateAllBirthdays();
    void connectChangeNotifier();
    bool processFetchRequest(QContactFetchRequest * const fetchRequest,
                             QContactAbstractRequest::State newState,
                             SyncMode syncMode = Incremental);
    void processNotificationQueues();
    void processNotifications(QList<TrackerChangeNotifier::Quad> &notifications,
                              QSet<QContactLocalId> &propertyChanges,
                              QSet<QContactLocalId> &resourceChanges);
    void fetchContacts(const QList<QContactLocalId> &contactIds);
    void fetchContacts(const QContactFilter &filter, const char *slot);
    void updateBirthdays(const QList<QContact> &changedBirthdays);
    void syncBirthdays(const QList<QContact> &birthdayContacts);

private:
    enum {
        NcoBirthDate,
        RdfType,
        NcoPersonContact,
        NcoContactGroup,
        NTrackerIds
    };

    QSparqlConnection &mSparqlConnection;
    QList<TrackerChangeNotifier::Quad> mDeleteNotifications;
    QList<TrackerChangeNotifier::Quad> mInsertNotifications;
    CDBirthdayCalendar *mCalendar;
    QContactManager *mManager;
    int mTrackerIds[NTrackerIds];
};

#endif // CDBIRTHDAYCONTROLLER_H
