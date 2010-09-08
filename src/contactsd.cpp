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

#include "contactsd.h"

#include "contactsdpluginloader.h"

#include <QSettings>
#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

/*!
  \class Contactsd

  \brief Main daemon class which manages remote contact plug-ins.

  \section y Populating the contact list with remote contacts
  Remote contacts are the user's buddies from various online sources, like
  social networks, and IM accounts set up on the device. We have
  The daemon is responsible to retrieve remote contacts information from various
  sources and synchronizes them with tracker as well as providing real-time
  updates of this information.
  The remote contacts information can be accessed using the QtMobility contacts
  API.
*/

/*!
  \fn void Contactsd::loadPlugins(const QStringList &plugins)

  Load plugins specified at \a plugins.

  \param loadPlugins The names of the plugins to load.
*/

/*!
  \fn QStringList Contactsd::loadedPlugins() const

  Return a list containing the name of all loaded plugins.

  \return A list of plugin names.
*/

Contactsd::Contactsd(QObject *parent)
    : QObject(parent),
      mLoader(new ContactsdPluginLoader())
{
}

Contactsd::~Contactsd()
{
    delete mLoader;
}

void Contactsd::loadPlugins(const QStringList &plugins)
{
    mLoader->loadPlugins(plugins);
}

QStringList Contactsd::loadedPlugins() const
{
    return mLoader->loadedPlugins();
}
