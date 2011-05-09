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

#include "debug.h"
#include "importstateconst.h"
#include "importstate.h"

using namespace Contactsd;

ImportState::ImportState()
    : mContactsAdded(0),
      mContactsMerged(0),
      mContactsRemoved(0),
      mStateStore(QSettings::IniFormat, QSettings::UserScope,
                  Contactsd::SettingsOrganization, Contactsd::SettingsApplication)
{
}

bool ImportState::hasActiveImports()
{
    return (mService2Accounts.size() != 0);
}

void ImportState::timeout()
{
    foreach (const QString &account, mService2Accounts.values()) {
        mStateStore.setValue(account, Contactsd::Imported);
    }

    mStateStore.sync();
    reset();
}

void ImportState::reset()
{
    mService2Accounts.clear();
    mContactsAdded = 0;
    mContactsMerged = 0;
    mContactsRemoved = 0;
}

QStringList ImportState::activeImportingServices()
{
    return mService2Accounts.uniqueKeys();
}

bool ImportState::serviceHasActiveImports(const QString &service)
{
    return mService2Accounts.contains(service);
}

void ImportState::addImportingAccount(const QString &service, const QString &account)
{
    debug() << Q_FUNC_INFO << service << account;

    if (not mService2Accounts.contains(service, account)) {
        mService2Accounts.insert(service, account);
        mStateStore.setValue(account, Contactsd::Importing);
        mStateStore.sync();
    }
}

bool ImportState::removeImportingAccount(const QString &service, const QString &account,
                                         int added, int removed, int merged)
{
    debug() << Q_FUNC_INFO << service << account;

    int numRemoved = mService2Accounts.remove(service, account);

    if (numRemoved) {
        mContactsAdded += added;
        mContactsRemoved += removed;
        mContactsMerged += merged;
        mStateStore.setValue(account, Contactsd::Imported);
        mStateStore.sync();
        return true;
    }
    else
        return false;
}

int ImportState::contactsAdded()
{
    return mContactsAdded;
}

int ImportState::contactsMerged()
{
    return mContactsMerged;
}

int ImportState::contactsRemoved()
{
    return mContactsRemoved;
}
