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
#include "importstate.h"

const QString telepathyString("telepathy");

void TestContactsd::init()
{
    mDaemon = new Contactsd(this);
}

void TestContactsd::testLoadAllPlugins()
{
    mDaemon->loadPlugins(QStringList());

    QStringList pluginList = mDaemon->loadedPlugins();
    QVERIFY2(pluginList.contains(telepathyString),
            QString("%1-plugin not Loaded!")
                .arg(telepathyString).toLatin1());
}

void TestContactsd::testLoadPlugins()
{
    mDaemon->loadPlugins(QStringList() << telepathyString);

    QStringList pluginList = mDaemon->loadedPlugins();
    QVERIFY2(pluginList.size() == 1,
            QString("%1 plugins loaded, expecting 1")
                .arg(pluginList.size()).toLatin1());
    QVERIFY2(pluginList.contains(telepathyString),
            QString("%1-plugin not Loaded!")
                .arg(telepathyString).toLatin1());
}

void TestContactsd::testImportState()
{
    ImportState state;

    QCOMPARE(state.hasActiveImports(), false);

    state.addImportingService("plugin1", "gtalk");
    QCOMPARE(state.hasActiveImports(), true);

    state.addImportingService("plugin1", "msn");
    QCOMPARE(state.hasActiveImports(), true);

    state.removeImportingService("plugin1", "gtalk", 10, 0, 3);
    QCOMPARE(state.hasActiveImports(), true);

    state.addImportingService("plugin2", "qq");
    state.removeImportingService("plugin1", "msn", 20, 1, 4);
    QCOMPARE(state.hasActiveImports(), true);

    state.removeImportingService("plugin2", "qq", 5, 0, 1);
    QCOMPARE(state.hasActiveImports(), false);

    QCOMPARE(state.contactsAdded(), 10+20+5);
    QCOMPARE(state.contactsRemoved(), 1);
    QCOMPARE(state.contactsMerged(), 3+4+1);

    state.reset();
    QCOMPARE(state.hasActiveImports(), false);
    QCOMPARE(state.contactsAdded(), 0);
    QCOMPARE(state.contactsRemoved(), 0);
    QCOMPARE(state.contactsMerged(), 0);
}

void TestContactsd::cleanup()
{
    delete mDaemon;
}

QTEST_MAIN(TestContactsd)
