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

#include <QSettings>
#include <QDBusConnection>
#include <QDBusError>

#include "contactsd.h"
#include "contactsdpluginloader.h"
#include "debug.h"

using namespace Contactsd;

/*!
  \class ContactsDaemon

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
  \fn void ContactsDaemon::loadPlugins(const QStringList &plugins)

  Load plugins specified at \a plugins.

  \param loadPlugins The names of the plugins to load.
*/

/*!
  \fn QStringList ContactsDaemon::loadedPlugins() const

  Return a list containing the name of all loaded plugins.

  \return A list of plugin names.
*/

ContactsDaemon::ContactsDaemon(QObject *parent)
    : QObject(parent),
      mLoader(new ContactsdPluginLoader())
{
}

ContactsDaemon::~ContactsDaemon()
{
    delete mLoader;
}

void ContactsDaemon::loadPlugins(const QStringList &plugins)
{
    mLoader->loadPlugins(plugins);
}

QStringList ContactsDaemon::loadedPlugins() const
{
    return mLoader->loadedPlugins();
}
