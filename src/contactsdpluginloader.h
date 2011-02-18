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

#ifndef CONTACTSPLUGINLOADER_H_
#define CONTACTSPLUGINLOADER_H_

#include <QObject>
#include <QMap>

#include "base-plugin.h"
#include "importstate.h"

class QTimer;
class QPluginLoader;
class QString;
class QStringList;

class ContactsdPluginLoader : public QObject
{
    Q_OBJECT

public:
    ContactsdPluginLoader();
    ~ContactsdPluginLoader();

    void loadPlugins(const QStringList &plugins);
    void loadPlugins(const QString &pluginsDir, const QStringList &plugins);
    QStringList loadedPlugins() const;
    bool registerNotificationService();

public Q_SLOTS:
    QStringList hasActiveImports();

Q_SIGNALS:
    void importStarted(const QString &service);
    void importStateChanged(const QString &finishedService,
                            const QString &newService);
    void importEnded(int contactsAdded, int contactsRemoved,
                     int contactsMerged);
    void pluginsLoaded();

private Q_SLOTS:
    void onPluginImportStarted(const QString &service, const QString &account);
    void onPluginImportEnded(const QString &service, const QString &account,
                             int contactsAdded, int contactsRemoved, int contactsMerged);
    void onImportTimeout();

private:
    void startImportTimer();
    void stopImportTimer();
    QString pluginName(Contactsd::BasePlugin *plugin);

    typedef QMap<QString, QPluginLoader *> PluginStore;
    PluginStore mPluginStore;
    ImportState mImportState;

    QTimer *mImportTimer;
};

#endif
