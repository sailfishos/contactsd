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

#include "contactsdpluginloader.h"

#include "contactsdplugininterface.h"

#include <QDebug>
#include <QDir>
#include <QPluginLoader>
#include <QVariant>

ContactsdPluginLoader::ContactsdPluginLoader()
{
    (void) new ImportNotifierAdaptor(this);
    registerNotificationService();
}

ContactsdPluginLoader::~ContactsdPluginLoader()
{
    PluginStore::const_iterator it = mPluginStore.constBegin();
    PluginStore::const_iterator end = mPluginStore.constEnd();
    while (it != end) {
        it.value()->unload();
        it.value()->deleteLater();
        ++it;
    }
    mPluginStore.clear();
}

void ContactsdPluginLoader::loadPlugins(const QStringList &plugins)
{
    QStringList pluginsDirs;
    QString pluginsDirsEnv = QString::fromLocal8Bit(
            qgetenv("CONTACTSD_PLUGINS_DIRS"));
    if (pluginsDirsEnv.isEmpty()) {
        pluginsDirs << CONTACTSD_PLUGINS_DIR;
    } else {
        pluginsDirs << pluginsDirsEnv.split(':');
    }

    foreach (const QString &pluginsDir, pluginsDirs) {
        loadPlugins(pluginsDir, plugins);
    }
}

void ContactsdPluginLoader::loadPlugins(const QString &pluginsDir,
        const QStringList &plugins)
{
    QPluginLoader *loader;
    QDir dir(pluginsDir);
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    foreach (const QString &fileName, dir.entryList()) {
        QString absFileName(dir.absoluteFilePath(fileName));
        qDebug() << "Trying to load plugin" << absFileName;
        loader = new QPluginLoader(absFileName);
        if (!loader->load()) {
            qWarning() << "Error loading plugin" << absFileName <<
                "-" << loader->errorString();
            delete loader;
            continue;
        }

        QObject *basePlugin = loader->instance();
        ContactsdPluginInterface *plugin =
            qobject_cast<ContactsdPluginInterface *>(basePlugin);
        if (!plugin) {
            qWarning() << "Error loading plugin" << absFileName << "- does not "
                "implement ContactsdPluginInterface";
            loader->unload();
            delete loader;
            continue;
        }

        ContactsdPluginInterface::PluginMetaData metaData = plugin->metaData();
        if (!metaData.contains(CONTACTSD_PLUGIN_NAME)) {
            qWarning() << "Error loading plugin" << absFileName <<
                "- invalid plugin metadata";
            loader->unload();
            delete loader;
            continue;
        }

        QString pluginName = metaData[CONTACTSD_PLUGIN_NAME].toString();
        if (!plugins.isEmpty() && !plugins.contains(pluginName)) {
            qWarning() << "Ignoring plugin" << absFileName;
            loader->unload();
            delete loader;
            continue;
        }

        if (mPluginStore.contains(pluginName)) {
            qWarning() << "Ignoring plugin" << absFileName <<
                "- plugin with name" << pluginName << "already registered";
            loader->unload();
            delete loader;
            continue;
        }

        qDebug() << "Plugin" << pluginName << "loaded";
        mPluginStore.insert(pluginName, loader);
        connect(basePlugin,
                SIGNAL(importStarted()),
                SIGNAL(importStarted()));
        connect(basePlugin,
                SIGNAL(importEnded(int, int, int)),
                SIGNAL(importEnded(int, int, int)));
        plugin->init();
    }
}

QStringList ContactsdPluginLoader::loadedPlugins() const
{
    return mPluginStore.keys();
}

bool ContactsdPluginLoader::hasActiveImports()
{
    foreach (const QString &pluginName, loadedPlugins()) {
        QPluginLoader *loader = mPluginStore[pluginName];
        QObject *basePlugin = loader->instance();
        ContactsdPluginInterface *plugin =
            qobject_cast<ContactsdPluginInterface *>(basePlugin);
        if (plugin->hasActiveImports()) {
            return true;
        }
    }
    return false;
}

void ContactsdPluginLoader::registerNotificationService()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qWarning() << "Could not connect to DBus:" << connection.lastError();
    }

    if (!connection.registerService("com.nokia.contacts.importprogress")) {
        qWarning() << "Could not register DBus service "
            "'com.nokia.contacts.importprogress':" << connection.lastError();
    }

    if (!connection.registerObject("/", this)) {
        qWarning() << "Could not register DBus object '/':" <<
            connection.lastError();
    }
}
