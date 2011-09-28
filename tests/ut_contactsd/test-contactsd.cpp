/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#include "test-contactsd.h"
#include "importstate.h"
#include <test-common.h>
#include <QtDBus>
#include <QByteArray>

const QString telepathyString("telepathy");

void TestContactsd::init()
{
    mLoader = new ContactsdPluginLoader();
}

void TestContactsd::envTest()
{
    qputenv("CONTACTSD_PLUGINS_DIRS", "/usr/lib/contactsd-1.0/plugins/:/usr/lib/contactsd-1.0/plgins/");
    mLoader->loadPlugins(QStringList());
    QVERIFY2(mLoader->loadedPlugins().count() > 0, "failed to load plugins from evn variable");
    qDebug() << mLoader->loadedPlugins();
    qputenv("CONTACTSD_PLUGINS_DIRS", "");
    mLoader->loadPlugins(QStringList());
}

void TestContactsd::instanceTest()
{
    ContactsdPluginLoader * daemon = new ContactsdPluginLoader();
    daemon->~ContactsdPluginLoader();
    QVERIFY2(daemon,0);
}

void TestContactsd::importNonPlugin()
{
    /* This is just a coverage test */
    const QString path(QDir::currentPath() + "/data/");
    qputenv("CONTACTSD_PLUGINS_DIRS", path.toAscii());
    mLoader->loadPlugins(QStringList());
    QStringList pluginList = mLoader->loadedPlugins();
    QCOMPARE(pluginList.size(), 0);
    qputenv("CONTACTSD_PLUGINS_DIRS", "");
}

void TestContactsd::importTest()
{
    const QString host("com.nokia.contactsd");
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface *interface = new QDBusInterface("com.nokia.contactsd",
            "/","com.nokia.contacts.importprogress",bus,this);
    QDBusReply<QStringList> result = interface->call("hasActiveImports");
    QVERIFY2(result.isValid() == true, result.error().message().toLatin1());
    QCOMPARE(result.value().count(), 0);
}

void TestContactsd::testFakePlugin()
{
    mLoader->loadPlugins(QStringList() << "fakeplugin");

    QStringList pluginList = mLoader->loadedPlugins();
    QVERIFY2(pluginList.size() == 0,
            QString("%1 plugins loaded, expecting 0")
                .arg(pluginList.size()).toLatin1());
    QVERIFY2(not pluginList.contains("fakeplugin"),
            QString("%1-plugin not Loaded!")
                .arg(telepathyString).toLatin1());
}

void TestContactsd::testDbusPlugin()
{
    mLoader->loadPlugins(QStringList() << "dbusplugin");

    QStringList pluginList = mLoader->loadedPlugins();
    QVERIFY2(pluginList.size() == 1,
            QString("%1 plugins loaded, expecting 1")
                .arg(pluginList.size()).toLatin1());
    QVERIFY2(pluginList.contains("dbusplugin"),
            QString("%1-plugin not Loaded!")
                .arg(telepathyString).toLatin1());
}

void TestContactsd::testLoadAllPlugins()
{
    mLoader->loadPlugins(QStringList());

    QStringList pluginList = mLoader->loadedPlugins();
    QVERIFY2(pluginList.contains(telepathyString),
            QString("%1-plugin not Loaded!")
                .arg(telepathyString).toLatin1());
}

void TestContactsd::testLoadPlugins()
{
    mLoader->loadPlugins(QStringList() << telepathyString);

    QStringList pluginList = mLoader->loadedPlugins();
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

    state.addImportingAccount("gtalk", "gtalk-account1");
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("gtalk"), true);

    state.addImportingAccount("msn", "msn-account1");
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("msn"), true);
    QCOMPARE(state.serviceHasActiveImports("gtalk"), true);

    QStringList activeServices = state.activeImportingServices();
    QCOMPARE(activeServices.size(), 2);
    QVERIFY(activeServices.contains("gtalk"));
    QVERIFY(activeServices.contains("msn"));

    state.removeImportingAccount("gtalk", "gtalk-account1", 10, 0, 3);
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("gtalk"), false);
    QCOMPARE(state.serviceHasActiveImports("msn"), true);

    QCOMPARE(state.activeImportingServices().size(), 1);

    state.addImportingAccount("msn", "msn-account2");
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("msn"), true);

    state.addImportingAccount("qq", "qq-account1");
    state.removeImportingAccount("msn", "msn-account1", 20, 1, 4);
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("msn"), true);
    QCOMPARE(state.serviceHasActiveImports("qq"), true);

    state.removeImportingAccount("qq", "qq-account1", 5, 0, 1);
    QCOMPARE(state.hasActiveImports(), true);
    QCOMPARE(state.serviceHasActiveImports("qq"), false);
    QCOMPARE(state.serviceHasActiveImports("msn"), true);

    state.removeImportingAccount("msn", "msn-account2", 2, 0, 1);
    QCOMPARE(state.hasActiveImports(), false);
    QCOMPARE(state.serviceHasActiveImports("msn"), false);
    QCOMPARE(state.activeImportingServices().size(), 0);

    QCOMPARE(state.contactsAdded(), 10+20+5+2);
    QCOMPARE(state.contactsRemoved(), 1);
    QCOMPARE(state.contactsMerged(), 3+4+1+1);

    state.reset();
    QCOMPARE(state.hasActiveImports(), false);
    QCOMPARE(state.contactsAdded(), 0);
    QCOMPARE(state.contactsRemoved(), 0);
    QCOMPARE(state.contactsMerged(), 0);
}

void TestContactsd::testDbusRegister()
{
    QVERIFY2(not mLoader->registerNotificationService(),
            "Re registry failed as expected");
}

void TestContactsd::testInvalid()
{
    mLoader->loadPlugins(QStringList() << "invalid");

    QStringList pluginList = mLoader->loadedPlugins();
    QVERIFY2(pluginList.size() == 0,
            QString("%1 plugins loaded, expecting 0")
                .arg(pluginList.size()).toLatin1());
    QVERIFY2(not pluginList.contains(telepathyString),
            QString("%1-plugin not Loaded!")
                .arg(telepathyString).toLatin1());
}
void TestContactsd::cleanup()
{
    if (mLoader)
        delete mLoader;
}

CONTACTSD_TEST_MAIN(TestContactsd)
