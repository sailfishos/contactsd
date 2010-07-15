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

// Qt
#include <QObject>
#include <QMap>

class QPluginLoader;
class QString;
class QStringList;

/*!
  \class ContacsdPluginLoader

  \brief Plugin loader and repository for contacsdaemon
*/

class ContactsdPluginLoader : public QObject
{
    Q_OBJECT
public:
    ContactsdPluginLoader(const QStringList &plugins, const QStringList &banList);
    ~ContactsdPluginLoader();
    QStringList validPlugins() const;

private:
    typedef QMap<QString, QPluginLoader*> PluginStore;
    PluginStore m_pluginStore;

    void loadPlugins(const QStringList &plugins, const QStringList &banList);
    bool reloadPlugin(const QString &plugin);

Q_SIGNALS:
    void pluginsLoaded();

private Q_SLOTS:
    void onPluginError(const QString &, const QString &, const QString &);
};
#endif
