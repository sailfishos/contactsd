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
#include <QMap>
#include <QPluginLoader>
#include <QTimer>
#include <QVariant>

const QString pluginPath = CONTACTSD_PLUGIN_PATH;

void ContactsdPluginLoader::loadPlugins(const QStringList &plugins, const QStringList &banList)
{
    QDir dir(pluginPath);
    QPluginLoader *loader(0);

    // FIXME: Remove hardcoded suffix
    dir.setNameFilters(QStringList() << QString::fromLatin1("*.so"));
    foreach (QString pluginFile, dir.entryList()) {
        qDebug() << Q_FUNC_INFO << "Trying to load plugin" << dir.absoluteFilePath(pluginFile);
        loader = new QPluginLoader(dir.absoluteFilePath(pluginFile));
        if (!loader->load()) {
            qWarning() << Q_FUNC_INFO << "Failed to load plugin" << loader->errorString();
            continue;
        }

        QObject *base_plugin = loader->instance();
        ContactsdPluginInterface *plugin = qobject_cast<ContactsdPluginInterface *>(base_plugin);
        ContactsdPluginInterface::PluginMetaData metaData = plugin->metaData();
        ContactsdPluginInterface::PluginMetaData::const_iterator it;
        it = metaData.find(CONTACTSD_PLUGIN_NAME);
        if (it == metaData.end()) {
            qWarning() << Q_FUNC_INFO << ": Invalid plugin meta-data for" << loader->fileName();
            loader->unload();
            delete loader;
            loader = 0;
            continue;
        }

        if ((plugins.isEmpty() || plugins.contains(it->toString())) &&
                !banList.contains(it->toString())) {
            m_pluginStore.insert(it->toString(), loader);
            plugin->init();
            // this might print a warning on console but it is
            // intentional
            connect (base_plugin, SIGNAL (error (const QString &, const QString &, const QString &)),
                    this, SLOT (onPluginError (const QString &, const QString &, const QString &)),
                    Qt::UniqueConnection);
        } else {
            loader->unload();
            delete loader;
            loader = 0;
        }
    }
}

ContactsdPluginLoader::ContactsdPluginLoader(const QStringList &plugins, const QStringList &banList)
{
    loadPlugins(plugins, banList);
    Q_EMIT pluginsLoaded();
}

ContactsdPluginLoader::~ContactsdPluginLoader()
{
    PluginStore::const_iterator it;
    it = m_pluginStore.begin();
    for(;it != m_pluginStore.end(); ++it) {
        it.value()->unload();
        it.value()->deleteLater();
    }
    m_pluginStore.clear();
}

void ContactsdPluginLoader::onPluginError(const QString &name, const QString &error, const QString &message)
{
    qWarning() << Q_FUNC_INFO << ": Plugin" << name << "signalized error" << error << "." << message;

    reloadPlugin (name);
}

const QStringList ContactsdPluginLoader::validPlugins() const
{
    return m_pluginStore.keys();
}

bool ContactsdPluginLoader::reloadPlugin(const QString &plugin)
{
    PluginStore::const_iterator it;
    it = m_pluginStore.find(plugin);
    if (it == m_pluginStore.end()) {
        return false;
    }

    if (!it.value()->unload()) {
        return false;
    }

    if (!it.value()->load()) {
        return false;
    }

    ContactsdPluginInterface *instance = qobject_cast<ContactsdPluginInterface *>(it.value()->instance());
    instance->init();

    return true;
}
