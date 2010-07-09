/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */
#ifndef CONTACTSPLUGINLOADER_H_
#define CONTACTSPLUGINLOADER_H_

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

class QPluginLoader;

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
    const QStringList validPlugins() const;

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
