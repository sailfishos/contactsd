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

#ifndef CONTACTSDPLUGININTERFACE_H
#define CONTACTSDPLUGININTERFACE_H

#include <QObject>
#include <QString>

class QVariant;
template<class Key, class Value> class QMap;
class QStringList;

const QString CONTACTSD_PLUGIN_VERSION = QString::fromLatin1("version");
const QString CONTACTSD_PLUGIN_NAME    = QString::fromLatin1("name");
const QString CONTACTSD_PLUGIN_COMMENT = QString::fromLatin1("comment");

class ContactsdPluginInterface
{
public:
    typedef QMap<QString, QVariant> PluginMetaData;

    virtual ~ContactsdPluginInterface () { };
    virtual void init() = 0;
    virtual PluginMetaData metaData() = 0;

    // deprecated - a trivial implementation is given to preserve ABI
    virtual bool hasActiveImports() const { return false; };

/* The plugin that wants to provide contacts importing feature
   must decalre the following signals:
Q_SIGNALS:
    // \param service - display name of a service (e.g. Gtalk, MSN)
    // \param account - account id or account path that can uniquely identify an account
    void importStarted(const QString &service, const QString &account);
    void importEnded(const QString &service, const QString &account,
                     int contactsAdded, int contactsRemoved, int contactsMerged);

   This is an ugly hack but we can't make ContactsdPluginInterface an QObject
   since the meta-objects would not be defined in this interface, and plguin
   will not be able to load. */
};

Q_DECLARE_INTERFACE(ContactsdPluginInterface, "com.nokia.contactsd.Plugin/1.0")

#endif // CONTACTSDPLUGININTERFACE_H
