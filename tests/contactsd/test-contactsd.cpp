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

#include "test-contactsd.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>

const QString telepathyString("telepathy");

const QString telepathyFile("libtelepathycollectorplugin.so");

//const QString login(""); Set your own values if needed
//const QString passwd("");
const QString pollingInterval("1000"); // Interval in seconds, big enough for now as we dont test this,
                                       // it has some issues if interval is too short

void TestContactsd::initTestCase()
{
    daemon = new Contactsd(this);
}

void TestContactsd::testLoadAllPlugins()
{
    daemon->loadPlugins(QStringList());

    QStringList pluginList = daemon->loadedPlugins();
    QVERIFY2(pluginList.contains(telepathyString), QString("%1-plugin not Loaded!").arg(telepathyString).toLatin1());
}



void TestContactsd::testLoadPlugins()
{
    delete daemon; // No other way to unload plugins

    daemon = new Contactsd(this);
    daemon->loadPlugins(QStringList() << telepathyString);
    QStringList pluginList = daemon->loadedPlugins();
    QVERIFY2(pluginList.size() == 1, QString("%1 plugins loaded, expecting 1").arg(pluginList.size()).toLatin1());
    QVERIFY2(pluginList.contains(telepathyString), QString("%1-plugin not Loaded!").arg(telepathyString).toLatin1());
}

void TestContactsd::cleanupTestCase()
{
    delete daemon;
    daemon=0;
}

QTEST_MAIN(TestContactsd)
