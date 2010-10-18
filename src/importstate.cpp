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

ImportState::ImportState()
    : mContactsAdded(0),
      mContactsMerged(0),
      mContactsRemoved(0)
{
}

bool ImportState::hasActiveImports()
{
    return (mImportingPlugin2Services.size() != 0);
}

bool ImportState::reset()
{
    if (hasActiveImports())
        return false;

    mImportingPlugin2Services.clear();
    mService2Accounts.clear();
    mContactsAdded = 0;
    mContactsMerged = 0;
    mContactsRemoved = 0;
    return true;
}


bool ImportState::serviceHasActiveImports(const QString &pluginName, const QString &service)
{
    return mImportingPlugin2Services.contains(pluginName, service);
}

void ImportState::addImportingAccount(const QString &pluginName, const QString &service,
                                      const QString &account)
{
    if (not mImportingPlugin2Services.contains(pluginName, service))
        mImportingPlugin2Services.insert(pluginName, service);

    if (not mService2Accounts.contains(service, account))
        mService2Accounts.insert(service, account);
}

void ImportState::removeImportingAccount(const QString &pluginName, const QString &service,
                                         const QString &account, int added, int removed, int merged)
{
    int numRemoved = mService2Accounts.remove(service, account);

    if (numRemoved) {
        mContactsAdded += added;
        mContactsRemoved += removed;
        mContactsMerged += merged;

        // If this service doesn't have any other active importing account,
        // we consider this service has finished importing
        if (not mService2Accounts.contains(service))
            mImportingPlugin2Services.remove(pluginName, service);
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
