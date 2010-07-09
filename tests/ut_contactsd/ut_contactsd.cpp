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

#include "ut_contactsd.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>


const QString telepathyString("telepathy");
const QString ringString("ring");

const QString telepathyFile("libtelepathycollectorplugin.so");

//const QString login(""); Set your own values if needed
//const QString passwd("");
const QString pollingInterval("1000"); // Interval in seconds, big enough for now as we dont test this,
                                       // it has some issues if interval is too short


void loopForEvents(ut_contactsd* ut_daemon)
{
    for(int i = 0; i < 20; i++)
        {
            usleep(100000);
            QCoreApplication::processEvents();
            if(ut_daemon->getPluginsLoaded())
                break;
        }
};


void ut_contactsd::initTestCase()
{
    daemon = new ContactsDaemon(this);

    connect(daemon, SIGNAL(pluginsLoaded()), this, SLOT(pluginsLoaded()) );
}

void ut_contactsd::testLoadAllPlugins()
{
    plugsLoaded=false;
    daemon->loadAllPlugins();

    loopForEvents(this);

    QVERIFY2((plugsLoaded), "Plugins were not loaded!");

    QStringList pluginList = daemon->validPlugins();
    QVERIFY2(pluginList.contains(telepathyString), QString("%1-plugin not Loaded!").arg(telepathyString).toLatin1());
    QVERIFY2(pluginList.contains(ringString), QString("%1-plugin not Loaded!").arg(ringString).toLatin1());
}



void ut_contactsd::testLoadPlugins()
{
    disconnect(daemon, SIGNAL(pluginsLoaded()));
    delete daemon; // No other way to unload plugins
    plugsLoaded=false;

    daemon = new ContactsDaemon(this);
    connect(daemon, SIGNAL(pluginsLoaded()), this, SLOT(pluginsLoaded()) );
    daemon->loadAllPlugins();
    loopForEvents(this);
    QVERIFY2((plugsLoaded), "Plugins were not loaded!");
}

void ut_contactsd::pluginsLoaded()
{
    plugsLoaded=true;
}

void ut_contactsd::cleanupTestCase()
{
    disconnect(daemon, SIGNAL(pluginsLoaded()));
    delete daemon;
    daemon=0;
}

QTEST_MAIN(ut_contactsd)
