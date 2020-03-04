/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2020 Jolla Ltd.
 ** Copyright (c) 2020 Open Mobile Platform LLC.
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **/

#ifndef CDCALENDARCONTROLLER_H
#define CDCALENDARCONTROLLER_H

#include <QObject>
#include <Accounts/Manager>
#include <extendedstorage.h>

class CDCalendarController : public QObject
{
    Q_OBJECT

public:
    explicit CDCalendarController(QObject *parent = 0);
    ~CDCalendarController();

public Q_SLOTS:
    void enabledEventCalDav(Accounts::AccountId id);
    void enabledEventSync(Accounts::AccountId id);

private:
    Accounts::Manager * SetupManager(
            const QString &service,
            void (CDCalendarController::*enabledEvent)(Accounts::AccountId id));

private:
    Accounts::Manager *m_manager_caldav;
    Accounts::Manager *m_manager_sync;
};

#endif // CDCALENDARCONTROLLER_H
