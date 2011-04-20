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

#ifndef CONTACTSD_BASE_PLUGIN_H
#define CONTACTSD_BASE_PLUGIN_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QStringList>
#include <QMap>


namespace Contactsd
{

class BasePlugin : public QObject
{
    Q_OBJECT

public:
    static const QString metaDataKeyVersion;
    static const QString metaDataKeyName;
    static const QString metaDataKeyComment;
    typedef QMap<QString, QVariant> MetaData;

    virtual ~BasePlugin () { };
    virtual void init() = 0;
    virtual MetaData metaData() = 0;

Q_SIGNALS:
    // \param service - display name of a service (e.g. Gtalk, MSN)
    // \param account - account id or account path that can uniquely identify an account
    void importStarted(const QString &service, const QString &account);
    void importEnded(const QString &service, const QString &account,
                     int contactsAdded, int contactsRemoved, int contactsMerged);
    void error(int code, const QString &message);
};

} // Contactsd

#endif // CONTACTSD_PLUGININTERFACE_H
