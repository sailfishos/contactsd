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

#include "test-telepathy-plugin.h"

#include <QMap>
#include <QPair>

#include <QtTracker/Tracker>
#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nie.h>
#include <qcontactfilters.h>
#include <qtcontacts.h>

#include "lib/simple-account-manager.h"
#include "lib/simple-account.h"
#include "lib/contacts-conn.h"

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "what/ev/er"

TestTelepathyPlugin::TestTelepathyPlugin(QObject *parent = 0) : Test(parent), manager(0)
{
}

void TestTelepathyPlugin::initTestCase()
{
    initTestCaseImpl();

    g_type_init();
    g_set_prgname("test-telepathy-plugin");
    tp_debug_set_flags("all");
    dbus_g_bus_get(DBUS_BUS_STARTER, 0);

    /* Create a QContactManager and track added/changed contacts */
    manager = new QContactManager(QLatin1String("tracker"));
    connect(manager, SIGNAL(contactsAdded(const QList<QContactLocalId>&)), this, SLOT(contactsAdded(const QList<QContactLocalId>&)));
    connect(manager, SIGNAL(contactsChanged(const QList<QContactLocalId>&)), this, SLOT(contactsChanged(const QList<QContactLocalId>&)));

    /* Create a fake AccountManager */
    TpDBusDaemon *dbus = tp_tests_dbus_daemon_dup_or_die();
    mAccountManagerService = tp_tests_object_new_static_class(TP_TESTS_TYPE_SIMPLE_ACCOUNT_MANAGER, NULL);
    tp_dbus_daemon_register_object(dbus, TP_ACCOUNT_MANAGER_OBJECT_PATH, mAccountManagerService);

    /* Create a fake Account */
    mAccountService = tp_tests_object_new_static_class(TP_TESTS_TYPE_SIMPLE_ACCOUNT, NULL);
    tp_dbus_daemon_register_object(dbus, ACCOUNT_PATH, mAccountService);

    /* Create a fake Connection */
    gchar *name;
    gchar *connPath;
    GError *error = 0;

    mConnService = TP_TESTS_CONTACTS_CONNECTION(g_object_new(
            TP_TESTS_TYPE_CONTACTS_CONNECTION,
            "account", "me@example.com",
            "protocol", "simple",
            NULL));
    QVERIFY(mConnService != 0);
    QVERIFY(tp_base_connection_register(TP_BASE_CONNECTION(mConnService), "contacts", &name,
                &connPath, &error));
    QVERIFY(error == 0);

    QVERIFY(name != 0);
    QVERIFY(connPath != 0);

    mConnName = QLatin1String(name);
    mConnPath = QLatin1String(connPath);

    g_free(name);
    g_free(connPath);

    mConn = Connection::create(mConnName, mConnPath);

    QCOMPARE(mConn->isReady(), false);
    QVERIFY(connect(mConn->requestConnect(),
                    SIGNAL(finished(Tp::PendingOperation*)),
                    SLOT(expectSuccessfulCall(Tp::PendingOperation*))));
    QCOMPARE(mLoop->exec(), 0);
    QCOMPARE(mConn->isReady(), true);

    QCOMPARE(mConn->status(), Connection::StatusConnected);

    /* Make the fake AccountManager announce the fake Account */
    /* FIXME: TpTestsSimpleAccountManager should have an API for this */
    tp_svc_account_manager_emit_account_validity_changed(mAccountManagerService, ACCOUNT_PATH, TRUE);

    /* Make the fake Account announce the fake Connection */
    /* FIXME: TpTestsSimpleAccount should have an API for this */
    GHashTable *change = tp_asv_new(NULL, NULL);
    tp_asv_set_object_path(change, "Connection", mConn->objectPath());
    tp_asv_set_uint32 (change, "ConnectionStatus",
        TP_CONNECTION_STATUS_CONNECTED);
    tp_asv_set_uint32 (change, "ConnectionStatusReason",
        TP_CONNECTION_STATUS_REASON_REQUESTED);
    tp_svc_account_emit_account_property_changed (mAccountService, change);
    g_hash_table_unref(change);

    g_object_unref (dbus);
}

void TestTelepathyPlugin::testTrackerImport()
{
    /* Create a new contact in fake CM */
    TpHandleRepoIface *serviceRepo =
        tp_base_connection_get_handles(mConnService, TP_HANDLE_TYPE_CONTACT);
    UIntList handles = UIntList() << tp_handle_ensure(serviceRepo, "alice", NULL, NULL);
    QVERIFY(handles[0] != 0);

    /* Set some info on the contact */
    const char *aliases[] = { "Alice" };
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        3, handles.toVector().constData(),
        aliases);

    /* TODO: Add the contact in ContactList, so contactsd should push it to tracker */
    QCOMPARE(mLoop->exec(), 0);

    /* Verify the contact saved in Tracker has correct information */
    QContact contact = manager->contact(added[0]);
    QCOMPARE(contact.displayLabel(), aliases[0]);
}

void TestTelepathyPlugin::contactsAdded(const QList<QContactLocalId>& contactIds)
{
    qDebug() << Q_FUNC_INFO << contactIds;
    added.append(contactIds);
    mLoop->exit(0);
}

void TestTelepathyPlugin::contactsChanged(const QList<QContactLocalId>& contactIds)
{
    qDebug() << Q_FUNC_INFO << contactIds;
    changed.append(contactIds);
    mLoop->exit(0);
}

void TestTelepathyPlugin::cleanupTestCase()
{
    g_object_unref(mAccountManagerService);
    g_object_unref(mAccountService);
    g_object_unref(mConnService);

    if (mConn) {
        // Disconnect and wait for the readiness change
        QVERIFY(connect(mConn->requestDisconnect(),
                    SIGNAL(finished(Tp::PendingOperation*)),
                    SLOT(expectSuccessfulCall(Tp::PendingOperation*))));
        QCOMPARE(mLoop->exec(), 0);

        if (mConn->isValid()) {
            QVERIFY(connect(mConn.data(),
                        SIGNAL(invalidated(Tp::DBusProxy *, QString, QString)),
                        SLOT(expectConnInvalidated())));
            QCOMPARE(mLoop->exec(), 0);
        }
    }

    delete manager;

    cleanupTestCaseImpl();
}

void TestTelepathyPlugin::init()
{
    initImpl();
}

void TestTelepathyPlugin::cleanup()
{
    cleanupImpl();
}

QTEST_MAIN(TestTelepathyPlugin)
