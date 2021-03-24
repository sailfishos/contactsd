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

#include <QDir>
#include <QPluginLoader>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QTimer>

#include "contactsdpluginloader.h"
#include "contactsimportprogressadaptor.h"
#include "debug.h"

using namespace Contactsd;

// import timeout is 5 minutes
const int IMPORT_TIMEOUT = 5 * 60 * 1000;
// alive check timeout is 30 seconds
const int ALIVE_TIMEOUT = 30 * 1000;

ContactsdPluginLoader::ContactsdPluginLoader(QDBusConnection *connection)
    : mImportTimer(0)
    , mCheckAliveTimer(0)
    , mDBusConnection(connection)
    , mHaveRegisteredDBus(false)
{
}

ContactsdPluginLoader::~ContactsdPluginLoader()
{
    if (mHaveRegisteredDBus) {
        mDBusConnection->unregisterObject(QLatin1String("/"));
    }

    qDeleteAll(mPluginStore.values());
    mPluginStore.clear();
}

void ContactsdPluginLoader::loadPlugins(const QStringList &plugins)
{
    QStringList pluginsDirs;
    QString pluginsDirsEnv = QString::fromLocal8Bit(qgetenv("CONTACTSD_PLUGINS_DIRS"));

    if (pluginsDirsEnv.isEmpty()) {
        pluginsDirs << CONTACTSD_PLUGINS_DIR;
    } else {
        pluginsDirs << pluginsDirsEnv.split(':');
    }

    Q_FOREACH (const QString &pluginsDir, pluginsDirs) {
        loadPlugins(pluginsDir, plugins);
    }
}

void ContactsdPluginLoader::loadPlugins(const QString &pluginsDir, const QStringList &plugins)
{
    QDir dir(pluginsDir);
    dir.setFilter(QDir::Files | QDir::NoSymLinks);

    Q_FOREACH (const QString &fileName, dir.entryList()) {
        QString absFileName = dir.absoluteFilePath(fileName);
        qCDebug(lcContactsd) << "Trying to load plugin" << absFileName;

        // We intentionally leak this object, and never unload the plugin
        // When you load a plugin, you can't know what happens in the underlying
        // loaded libraries, so unloading a plugin while being sure it will not
        // have any unwanted side effect is just impossible. For ignored plugins,
        // we just avoid calling the init() function.
        QPluginLoader loader(absFileName);

        QObject *pluginObject = loader.instance();

        if (!pluginObject) {
            qCWarning(lcContactsd) << "Error loading plugin" << absFileName << "- " << loader.errorString();
            continue;
        }

        BasePlugin *basePlugin = qobject_cast<BasePlugin *>(pluginObject);

        if (!basePlugin) {
            qCWarning(lcContactsd) << "Error loading plugin" << absFileName << "- not a Contactd::BasePlugin";
            continue;
        }

        BasePlugin::MetaData metaData = basePlugin->metaData();

        if (!metaData.contains(BasePlugin::metaDataKeyName)) {
            qCWarning(lcContactsd) << "Error loading plugin" << absFileName << "- invalid plugin metadata";
            continue;
        }

        QString pluginName = metaData[BasePlugin::metaDataKeyName].toString();

        if (!plugins.isEmpty() && !plugins.contains(pluginName)) {
            qCWarning(lcContactsd) << "Ignoring plugin" << absFileName;
            continue;
        }

        if (mPluginStore.contains(pluginName)) {
            qCWarning(lcContactsd) << "Ignoring plugin" << absFileName <<
                "- plugin with name" << pluginName << "already registered";
            continue;
        }

        qCDebug(lcContactsd) << "Plugin" << pluginName << "loaded";
        mPluginStore.insert(pluginName, basePlugin);

        connect(basePlugin, SIGNAL(importStarted(const QString &, const QString &)),
                this, SLOT(onPluginImportStarted(const QString &, const QString &)));
        connect(basePlugin, SIGNAL(importEnded(const QString &, const QString &, int, int, int)),
                this, SLOT(onPluginImportEnded(const QString &, const QString &, int,int,int)));
        connect(basePlugin, SIGNAL(error(int, const QString &)),
                this, SIGNAL(error(int, const QString &)));
        connect(basePlugin, SIGNAL(importAlive()),
                this, SLOT(onImportAlive()));

        basePlugin->init();
    }
}

QStringList ContactsdPluginLoader::loadedPlugins() const
{
    return mPluginStore.keys();
}

QStringList ContactsdPluginLoader::hasActiveImports()
{
    return mImportState.activeImportingServices();
}

void ContactsdPluginLoader::onPluginImportStarted(const QString &service, const QString &account)
{
    BasePlugin *plugin = qobject_cast<BasePlugin *>(sender());
    if (not plugin) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "invalid Contactsd::BasePlugin object";
        return;
    }

    QString name = pluginName(plugin);
    qCDebug(lcContactsd) << Q_FUNC_INFO << "by plugin" << name
             << "with service" << service << "account" << account;

    if (mImportState.hasActiveImports()) {
        // check if any account from the same service is importing now
        if (not mImportState.serviceHasActiveImports(service)) {
            // there was no active import from this service, so we update import state with new services
            Q_EMIT importStateChanged(QString(), service);
        }
    } else {
        // new import
        mImportState.reset();
        startImportTimer();
        Q_EMIT importStarted(service);
    }

    mImportState.addImportingAccount(service, account);
}

void ContactsdPluginLoader::onPluginImportEnded(const QString &service, const QString &account,
                                                int contactsAdded, int contactsRemoved, int contactsMerged)
{
    BasePlugin *plugin = qobject_cast<BasePlugin *>(sender());
    if (not plugin) {
        qCWarning(lcContactsd) << Q_FUNC_INFO << "invalid Contactsd::BasePlugin object";
        return;
    }

    QString name = pluginName(plugin);
    qCDebug(lcContactsd) << Q_FUNC_INFO << "by plugin" << name
             << "service" << service << "account" << account
             << "added" << contactsAdded << "removed" << contactsRemoved
             << "merged" << contactsMerged;

    bool removed = mImportState.removeImportingAccount(service, account, contactsAdded,
                                                       contactsRemoved, contactsMerged);
    if (not removed) {
        qCDebug(lcContactsd) << Q_FUNC_INFO << "account does not exist";
        return;
    }

    if (mImportState.hasActiveImports()) {
        if (not mImportState.serviceHasActiveImports(service)) {
            // This service has no acive importing accounts anymore
            Q_EMIT importStateChanged(service, QString());
        }
    } else {
        stopImportTimer();
        stopCheckAliveTimer();
        Q_EMIT importEnded(mImportState.contactsAdded(), mImportState.contactsRemoved(),
                           mImportState.contactsMerged());
    }
}

void ContactsdPluginLoader::onImportAlive()
{
    if (0 != mCheckAliveTimer && mCheckAliveTimer->isActive()) {
        startImportTimer();
    }
}

void ContactsdPluginLoader::onImportTimeout()
{
    qCDebug(lcContactsd) << Q_FUNC_INFO;
    stopImportTimer();
    startCheckAliveTimer();
}

void ContactsdPluginLoader::startCheckAliveTimer()
{
    stopCheckAliveTimer();

    mCheckAliveTimer = new QTimer(this);
    connect(mCheckAliveTimer, SIGNAL(timeout()),
            this, SLOT(onCheckAliveTimeout()));
    mCheckAliveTimer->start(ALIVE_TIMEOUT);
}

void ContactsdPluginLoader::stopCheckAliveTimer()
{
    if (0 == mCheckAliveTimer)
        return;

    mCheckAliveTimer->stop();
    delete mCheckAliveTimer;
    mCheckAliveTimer = 0;
}

void ContactsdPluginLoader::onCheckAliveTimeout()
{
    stopCheckAliveTimer();

    if (0 == mImportTimer) {
        Q_EMIT importEnded(mImportState.contactsAdded(), mImportState.contactsRemoved(),
                           mImportState.contactsMerged());
        mImportState.timeout();
    }
}

void ContactsdPluginLoader::startImportTimer()
{
    stopImportTimer();

    // Add a timeout timer
    mImportTimer = new QTimer(this);
    connect(mImportTimer, SIGNAL(timeout()),
            this, SLOT(onImportTimeout()));
    mImportTimer->start(IMPORT_TIMEOUT);
}

void ContactsdPluginLoader::stopImportTimer()
{
    delete mImportTimer;
    mImportTimer = 0;
}

QString ContactsdPluginLoader::pluginName(BasePlugin *plugin)
{
    BasePlugin::MetaData metaData = plugin->metaData();
    return metaData[BasePlugin::metaDataKeyName].toString();
}

bool ContactsdPluginLoader::registerNotificationService()
{
    if (mHaveRegisteredDBus) {
        return true;
    }

    if (!mDBusConnection->registerObject("/", this)) {
        qCWarning(lcContactsd) << "Could not register DBus object '/':" <<
            mDBusConnection->lastError();
        return false;
    }

    mHaveRegisteredDBus = true;
    new ContactsImportProgressAdaptor(this);
    return true;
}
