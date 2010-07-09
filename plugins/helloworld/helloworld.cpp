/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#include "helloworld.h"
#include <QDebug>
#include <QtCore>


HelloWorld::HelloWorld()
    : mCache(0)
{
}

HelloWorld::~HelloWorld()
{
    delete mCache;
}

void HelloWorld::init()
{
    qDebug() << Q_FUNC_INFO << "Hello World" << "Plugins Has started" ;
}

QMap<QString, QVariant> HelloWorld::metaData()
{
    QMap<QString, QVariant> data;
    data[CONTACTSD_PLUGIN_NAME]    = QVariant(QString("hello"));
    data[CONTACTSD_PLUGIN_VERSION] = QVariant(QString("0.1"));
    data[CONTACTSD_PLUGIN_COMMENT] = QVariant(QString("Example plugin for the contactsd"));
    //TODO: translations ?
    return data;
}
Q_EXPORT_PLUGIN2(HwPlugin, HelloWorld)
