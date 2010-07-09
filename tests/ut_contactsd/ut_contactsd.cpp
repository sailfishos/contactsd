/* * This file is part of contacts *
 * Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

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
