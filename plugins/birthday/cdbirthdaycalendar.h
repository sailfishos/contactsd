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

#ifndef CDBIRTHDAYCALENDAR_H
#define CDBIRTHDAYCALENDAR_H

#include <QObject>
#include <QContact>
#include <QDate>

#include <extendedstorage.h>
#include <extendedcalendar.h>

QTM_USE_NAMESPACE

class CDBirthdayCalendar : public QObject
{
    Q_OBJECT

public:
    enum SyncMode {
        Incremental,
        FullSync
    };

    //! Constructor.
    explicit CDBirthdayCalendar(SyncMode syncMode, QObject *parent = 0);
    ~CDBirthdayCalendar();

    //! Updates and saves Birthday of \a contact to calendar.
    void updateBirthday(const QContact &contact);

    //! Deletes \a contact birthday from calendar.
    void deleteBirthday(QContactLocalId contactId);

    QDate birthdayDate(QContactLocalId contactId);
    QString summary(QContactLocalId contactId);

private:
    mKCal::Notebook::Ptr createNotebook();

    QString calendarEventId(QContactLocalId contactId);
    KCalCore::Event::Ptr calendarEvent(QContactLocalId contactId);

private Q_SLOTS:
    void onLocaleChanged();

private:
    mKCal::ExtendedCalendar::Ptr mCalendar;
    mKCal::ExtendedStorage::Ptr mStorage;
};

#endif // CDBIRTHDAYCALENDAR_H
