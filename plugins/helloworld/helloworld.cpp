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

#include "helloworld.h"

#include <QDebug>
#include <QSettings>
#include <QtPlugin>

HelloWorld::HelloWorld()
{
}

HelloWorld::~HelloWorld()
{
}

void HelloWorld::init()
{
    qDebug() << "Hello World" << "Plugins Has started" ;
}

QMap<QString, QVariant> HelloWorld::metaData()
{
    QMap<QString, QVariant> data;
    data[CONTACTSD_PLUGIN_NAME]    = QVariant(QString("hello"));
    data[CONTACTSD_PLUGIN_VERSION] = QVariant(QString("0.1"));
    data[CONTACTSD_PLUGIN_COMMENT] = QVariant(QString("Example plugin for the contactsd"));
    // TODO: translations ?
    return data;
}

bool HelloWorld::hasActiveImports() const
{
    return false;
}

Q_EXPORT_PLUGIN2(HwPlugin, HelloWorld)
