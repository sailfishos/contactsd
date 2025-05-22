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

#include <Accounts/Manager>
#include <Accounts/Service>

#include <extendedstorage.h>

#include "cdcalendarcontroller.h"
#include "debug.h"

using namespace Accounts;

namespace {
// Anonymous namespace

mKCal::Notebook::List notebooksFromId(const mKCal::Notebook::List &list, AccountId id)
{
    const QString account = QString::number(id);
    mKCal::Notebook::List notebooks;
    for (const mKCal::Notebook::Ptr &notebook : list) {
        if (account == notebook->account()) {
            notebooks.append(notebook);
        }
    }
    return notebooks;
}

/*!
    \brief Enable/disable given mKCal notebook

    Sets the \a enabled status for the given notebook.
*/
const QByteArray VISIBILITY_CHANGED_FLAG("hidden_by_account");
bool updateNotebook(mKCal::Notebook::Ptr notebook, bool enabled)
{
    if (notebook->isEnabled() != enabled) {
        notebook->setIsEnabled(enabled);

        // Backward compatibility when visibility flag was flipped by enable status.
        if (!notebook->customProperty(VISIBILITY_CHANGED_FLAG).isEmpty()) {
            notebook->setCustomProperty(VISIBILITY_CHANGED_FLAG, QString());
            notebook->setIsVisible(true);
        }
        return true;
    }
    return false;
}

bool isServiceEnabledByProfile(Manager *manager, AccountId id, const QString &syncProfile)
{
    Account *account = manager->account(id);
    if (!account) {
        return false;
    }
    account->selectService();
    if (!account->isEnabled()) {
        return false;
    }

    bool serviceEnabled = false;
    for (const Service &service : account->enabledServices()) {
        account->selectService(service);
        for (const QString &key : account->allKeys()) {
            if (key.endsWith(QLatin1String("/profile_id"))) {
                serviceEnabled = (account->valueAsString(key) == syncProfile);
                break;
            }
        }
        if (serviceEnabled) {
            break;
        }
    }
    account->selectService();
    return serviceEnabled;
}

void updateNotebooks(AccountId id = 0)
{
    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = calendar->defaultStorage(calendar);
    storage->open();
    // A generic manager is used to check for the enabled service
    // matching a profile_id key, since we don't know in advance
    // which service is associated with a notebook of a given account.
    Manager manager;
    for (AccountId accId : (id
                            ? (AccountIdList() << id)
                            : manager.accountList())) {
        for (mKCal::Notebook::Ptr notebook : notebooksFromId(storage->notebooks(), accId)) {
            qCDebug(lcContactsd) << "checking notebook" << notebook->name()
                                 << "enabled status for account" << accId;
            if (updateNotebook(notebook, isServiceEnabledByProfile(&manager, accId, notebook->syncProfile()))) {
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
void CDCalendarController::setupListener(const QString &serviceType)
{
    Manager * manager = new Manager(serviceType, this);

    Error error = manager->lastError();
    if (error.type() == Error::NoError) {
        connect(manager, &Manager::enabledEvent,
                this, [manager] (AccountId id) {
                          qCDebug(lcContactsd) << "enabled changed" << id << manager->serviceType();
                          updateNotebooks(id);
                      });
    } else {
        qCWarning(lcContactsd) << "Accounts manager creation failed for" << serviceType
                               << "with error:" << error.message();
    }
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
    setupListener(QStringLiteral("caldav"));
    setupListener(QStringLiteral("sync"));
    updateNotebooks();

    // Possible improvement: in case of a single service type
    // handling notebook (like "calendar"), the code could be
    // simplified to have a unique restricted manager of this
    // service type.
}

CDCalendarController::~CDCalendarController()
{
}
