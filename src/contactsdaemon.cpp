/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#include "contactsdaemon.h"
#include "contactsdpluginloader.h"

#include <QDebug>

#include <QMap>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include <QVariant>
#include <QSettings>

/*!
  \class ContactsDaemon

  \brief Main daemon class which managers remote contact plug-ins.

  \section y  Populating the contact list with remote contacts
  Remote contacts are the user's buddies from various online sources, like social networks, and IM accounts set up on the device. We have
  written a daemon process which gathers remote contacts from various sources and synchronizes them
  with tracker. And these contacts can be accessed using the QtMobility contacts API. Apart from
  this, the daemon also provides real-time updates of the remote contacts, and this can also be
  monitored using the mobility contacts API.

  \section x Setting up remote contact fetching.

  Start m-sb-session and start up mission-control and the contactsd. After that run contacts
  application. enter your gtalk id and password
  in the login dialog in the first page of the contacts application. Then after few seconds you
  should see your remote contacts
  in the contact list.

  The contacts daemon is located in /usr/bin/contactsd and normally it needs to be manually started
  inside scratchbox.

*/

/*!
  \fn ContactsDaemon::loadAllPlugins (const QStringList &loadPlugins)

  instructs the daemon to load all plugins, and after loading, daemon will also execute the plugins.
  \param loadPlugins - constraints loading to load and init only those plugins with metadata->("name") in loadPlugins list.

  \sa initPlugin(const QString&), initPlugins(), PersonCollectorInterface::init()
*/


/*!
  \fn ContactsDaemon::initPlugins()

  runs all the already loaded plugins by executing
  \l{ContacsdPluginInterface::init()}{initializes the plugin}.

  \sa initPlugin(const QString&)
*/

/*!

  \fn QStringList ContactsDaemon::validPlugins() const

  \return The list of valid plugins handled by the current daemon session.
*/


class ContactsDaemon::Private
{
public:
    Private():settings(0), loader(0) {}
    ~Private()
    {
        delete settings;
        delete loader;
    }
    QSettings * settings;
    ContactsdPluginLoader *loader;
    QStringList banList;
};

ContactsDaemon::ContactsDaemon(QObject* parent): QObject(parent),
        d(new Private)
{
    d->settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "Nokia","ContactsDaemon");
}

ContactsDaemon::~ContactsDaemon()
{
    delete d;
}

void ContactsDaemon::loadAllPlugins(const QStringList &loadPlugins)
{
    if( !d->loader ) {
        d->loader = new ContactsdPluginLoader(loadPlugins, d->banList);
        connect(d->loader, SIGNAL(pluginsLoaded()), this, SIGNAL(pluginsLoaded()));
    }
}

const QStringList ContactsDaemon::validPlugins() const
{
    if (d->loader != 0) {
        return d->loader->validPlugins();
    }
}
