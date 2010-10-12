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
    return (mPluginsImporting.size() != 0);
}

bool ImportState::reset()
{
    if (hasActiveImports())
        return false;

    mPluginsImporting.clear();
    mContactsAdded = 0;
    mContactsMerged = 0;
    mContactsRemoved = 0;
    return true;
}

void ImportState::addImportingServices(const QString &pluginName, const QStringList &services)
{
    QStringList &currentServices = mPluginsImporting[pluginName];
    currentServices.append(services);
    currentServices.removeDuplicates();
}

void ImportState::removeImportngServices(const QString &pluginName, const QStringList &services)
{
    if (!mPluginsImporting.contains(pluginName)) {
        qWarning() << Q_FUNC_INFO << pluginName << "does not have active contacts import";
        return ;
    }

    QStringList &currentServices = mPluginsImporting[pluginName];
    foreach (const QString &service, services)
        currentServices.removeAll(service);
}

void ImportState::pluginImportFinished(const QString &pluginName, int added,  int removed, int merged)
{
    mPluginsImporting.remove(pluginName);
    mContactsAdded += added;
    mContactsRemoved += removed;
    mContactsMerged += merged;
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
