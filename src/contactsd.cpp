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

#include <QCoreApplication>
#include <QSettings>
#include <QDBusConnection>
#include <QDBusError>
#include <QDir>

#include "contactsd.h"
#include "contactsdpluginloader.h"
#include "synctrigger.h"
#include "debug.h"

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

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

int ContactsDaemon::sigFd[2] = {0, 0};

ContactsDaemon::ContactsDaemon(QObject *parent)
    : QObject(parent),
      mDBusConnection(QDBusConnection::sessionBus()),
      mLoader(new ContactsdPluginLoader(&mDBusConnection)),
      mSyncTrigger(new SyncTrigger(&mDBusConnection))
{
    if (!mDBusConnection.isConnected()) {
        qCWarning(lcContactsd) << "Could not connect to DBus:" << mDBusConnection.lastError();
    } else if (!mDBusConnection.registerService(QStringLiteral("com.nokia.contactsd"))) {
        qCWarning(lcContactsd) << "Could not register DBus service "
            "'com.nokia.contactsd':" << mDBusConnection.lastError();
    } else if (!mLoader->registerNotificationService()) {
        qCWarning(lcContactsd) << "unable to register notification service";
    } else if (!mSyncTrigger->registerTriggerService()) {
        qCWarning(lcContactsd) << "unable to register sync trigger service";
    }

    // The UNIX signals call unixSignalHandler(), but that is not called
    // through the main loop. To process signals in the mainloop correctly,
    // we create a socket, and write a byte on one end when the UNIX signal
    // handler is invoked. This will trigger the QSocketNotifier::activated
    // signal (this time *through* the main loop) and call onUnixSignalReceived()
    // from where we can ask the QApplication to quit.
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd) != 0) {
        qCWarning(lcContactsd) << "Could not setup UNIX signal handler";
    }

    mSignalNotifier = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, this);
    connect(mSignalNotifier, SIGNAL(activated(int)), SLOT(onUnixSignalReceived()));
}

ContactsDaemon::~ContactsDaemon()
{
    delete mLoader;
    delete mSyncTrigger;
    mDBusConnection.unregisterService(QLatin1String("com.nokia.contactsd"));
}

void ContactsDaemon::loadPlugins(const QStringList &plugins)
{
    mLoader->loadPlugins(plugins);
}

QStringList ContactsDaemon::loadedPlugins() const
{
    return mLoader->loadedPlugins();
}

void ContactsDaemon::unixSignalHandler(int)
{
    // Write a byte on the socket to activate the socket listener
    char a = 1;
    if (::write(sigFd[0], &a, sizeof(a)) != sizeof(a)) {
        qCWarning(lcContactsd) << "Unable to write to sigFd" << errno;
    }
}

void ContactsDaemon::onUnixSignalReceived()
{
    // Mask signals
    mSignalNotifier->setEnabled(false);

    // Empty the socket buffer
    char dummy;
    if (::read(sigFd[1], &dummy, sizeof(dummy)) != sizeof(dummy)) {
        qCWarning(lcContactsd) << "Unable to complete read from sigFd" << errno;
    }

    qCDebug(lcContactsd) << "Received quit signal";

    QCoreApplication::quit();

    // Unmask signals
    mSignalNotifier->setEnabled(true);
}
