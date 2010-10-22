/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (people-users@projects.maemo.org)
**
** This file is part of contactsd.
**
** If you have questions regarding the use of this file, please contact
** Nokia at people-users@projects.maemo.org.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include <QDebug>

#include "importstate.h"
#include "importstateconst.h"

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

bool ImportState::reset()
{
    if (hasActiveImports())
        return false;

    mService2Accounts.clear();
    mContactsAdded = 0;
    mContactsMerged = 0;
    mContactsRemoved = 0;
    return true;
}

bool ImportState::serviceHasActiveImports(const QString &service)
{
    return mService2Accounts.contains(service);
}

void ImportState::addImportingAccount(const QString &service, const QString &account)
{
    if (not mService2Accounts.contains(service, account)) {
        mService2Accounts.insert(service, account);

        mStateStore.setValue(account, Contactsd::Importing);
        mStateStore.sync();
    }
}

void ImportState::removeImportingAccount(const QString &service, const QString &account,
                                         int added, int removed, int merged)
{
    int numRemoved = mService2Accounts.remove(service, account);

    if (numRemoved) {
        mContactsAdded += added;
        mContactsRemoved += removed;
        mContactsMerged += merged;

        mStateStore.setValue(account, Contactsd::Imported);
        mStateStore.sync();
    }
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
