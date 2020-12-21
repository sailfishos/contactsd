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

#include <Accounts/Service>

#include "cdcalendarcontroller.h"
#include "cdcalendarplugin.h"
#include "debug.h"

using namespace Contactsd;
using namespace Accounts;

namespace {
// Anonymous namespace

/*!
    \brief Enable/disable all mKCal notebooks related to the account id

    Sets the \a enabled status for all notebooks associated with the given
    account \a id.
*/
void updateNotebooks(AccountId id, bool enabled)
{
    mKCal::ExtendedCalendar::Ptr calendar = mKCal::ExtendedCalendar::Ptr(
                new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = calendar->defaultStorage(calendar);
    storage->open();

    const mKCal::Notebook::List notebooks = storage->notebooks();
    for (const mKCal::Notebook::Ptr notebook : notebooks) {
        const QString accountStr = notebook->account();
        bool ok = false;
        AccountId accountInt = accountStr.toULong(&ok);
        if (ok && accountInt == id) {
            bool visible = notebook->isVisible();
            if (visible != enabled) {
                notebook->setIsVisible(enabled);
                storage->updateNotebook(notebook);
            }
        }
    }
}

} // Anonymous namespace

/*!
    \brief Creates and connects an Account Manager

    Creates an Account Manager for the service type and attaches the
    manager's enabledEvent signal to the provided \a enabledEvent slot.
*/
Manager * CDCalendarController::SetupManager(
        const QString &service,
        void (CDCalendarController::*enabledEvent)(AccountId id))
{

    Manager * manager = new Manager(service, this);

    Error error = manager->lastError();
    if (error.type() == Error::NoError) {
        connect(manager, &Manager::enabledEvent, this, enabledEvent);
    }
    else {
        qWarning() << "Accounts manager creation failed for" << service
                   << "with error:" << error.message();
    }

    return manager;
}

/*!
    \instantiates CDCalendarController
    \brief Synchronises mKCal notebook enable state with Accounts

    The mKCal notebook enabled/disabled state is interpreted to mean the
    calendar should be considered available.

    This plugin watches for changes in the Account or Account Service enabled
    status for Accounts with calendars and updates the relevant notebooks to
    match.
*/
CDCalendarController::CDCalendarController(QObject *parent)
    : QObject(parent)
{
    // Managers are needed following the specific service types we're interested
    // in. Without a type, libaccounts-glib won't send any signals (if we do set
    // a type, it sends us signals for all account types!).
    m_manager_caldav = SetupManager(QStringLiteral("caldav"),
                                    &CDCalendarController::enabledEventCalDav);
    m_manager_sync = SetupManager(QStringLiteral("sync"),
                                  &CDCalendarController::enabledEventSync);
}

CDCalendarController::~CDCalendarController()
{
}

/*!
    \brief Responds to changes in the enabled state for CalDav services

    This is called when the enabled status for account with \a id changes.

    If the account has a relevant service, the notebooks associated with it
    have their enabled state set using the following recipe:

    "If the account is enabled and at least one calendar service is enabled,
    then the notebooks will be enabled. Otherwise they will be disabled."
*/
void CDCalendarController::enabledEventCalDav(AccountId id)
{
    Account *account = m_manager_caldav->account(id);

    const ServiceList serviceList = account->services();
    if (serviceList.isEmpty()) {
        // Even when the Account Manager is configured to be only interested
        // in accounts with a specific service type, libaccounts-glib will still
        // send events for other accounts, but their service list will be empty.
        // We want to ignore these events.
        return;
    }

    bool enabled = account->enabled();
    if (enabled) {
        // The account is enabled, but the calendar service may not be
        enabled = false;
        ServiceList::const_iterator it;
        for (it = serviceList.constBegin();
             !enabled && it != serviceList.constEnd(); ++it) {
            account->selectService(*it);
            enabled = account->enabled();
        }
        account->selectService();
    }

    // Update the mKCal notebook with the result
    updateNotebooks(id, enabled);
}

/*!
    \brief Responds to changes in the enabled state for Google services

    This is called when the enabled status for account with \a id changes.

    If this is a Google account with a relevant service, the notebooks
    associated with it have their enabled state set using the following recipe:

    "If the account is enabled and at least one calendar service is enabled,
    then the notebooks will be enabled. Otherwise they will be disabled."
*/
void CDCalendarController::enabledEventSync(AccountId id)
{
    Account *account = m_manager_sync->account(id);

    if (account->providerName() != QLatin1String("google")) {
        // We're only interested in Google accounts here
        return;
    }

    const ServiceList serviceList = account->services();
    if (serviceList.isEmpty()) {
        // Even when the Account Manager is configured to be only interested
        // in accounts with a specific service type, libaccounts-glib will still
        // send events for other accounts, but their service list will be empty.
        // We want to ignore these events.
        return;
    }

    bool enabled = account->enabled();
    if (enabled) {
        // The account is enabled, but the calendar service may not be
        enabled = false;
        ServiceList::const_iterator it;
        for (it = serviceList.constBegin();
             !enabled && it != serviceList.constEnd(); ++it) {
            // This seems to be the only way to check whether it's a calendar
            if ((*it).name() == QLatin1String("google-calendars")) {
                account->selectService(*it);
                enabled = account->enabled();
            }
        }
        account->selectService();
    }

    // Update the mKCal notebook with the result
    updateNotebooks(id, enabled);
}
