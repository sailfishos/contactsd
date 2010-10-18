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

    state.addImportingAccount("plugin1", "gtalk", "gtalk-account1");
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "gtalk"), true);

    state.addImportingAccount("plugin1", "msn", "msn-account1");
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "msn"), true);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "gtalk"), true);

    state.removeImportingAccount("plugin1", "gtalk", "gtalk-account1", 10, 0, 3);
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "gtalk"), false);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "msn"), true);

    state.addImportingAccount("plugin1", "msn", "msn-account2");
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "msn"), true);

    state.addImportingAccount("plugin2", "qq", "qq-account1");
    state.removeImportingAccount("plugin1", "msn", "msn-account1", 20, 1, 4);
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "msn"), true);
    QCOMPARE(state.serviceHasActiveImports("plugin2", "qq"), true);

    state.removeImportingAccount("plugin2", "qq", "qq-account1", 5, 0, 1);
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("plugin2", "qq"), false);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "msn"), true);

    state.removeImportingAccount("plugin1", "msn", "msn-account2", 2, 0, 1);
    QCOMPARE(state.hasActiveImports(), false);
    QCOMPARE(state.serviceHasActiveImports("plugin1", "msn"), false);

    QCOMPARE(state.contactsAdded(), 10+20+5+2);
    QCOMPARE(state.contactsRemoved(), 1);
    QCOMPARE(state.contactsMerged(), 3+4+1+1);

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
