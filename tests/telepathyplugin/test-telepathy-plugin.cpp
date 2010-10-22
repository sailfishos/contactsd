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

#include <TelepathyQt4/PendingReady>

#include <telepathy-glib/svc-account-manager.h>
#include <telepathy-glib/svc-account.h>

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "fakecm/fakeproto/fakeaccount"
#define BUS_NAME "org.maemo.Contactsd.UnitTest"

TestExpectation::TestExpectation():flags(All),
    presence(TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN)
{
}

void TestExpectation::verify(QContact &contact) const
{
    QCOMPARE(contact.details<QContactOnlineAccount>().count(), 1);
    QCOMPARE(contact.details<QContactPresence>().count(), 1);
    QCOMPARE(contact.detail<QContactOnlineAccount>().accountUri(), accountUri);
    QCOMPARE(contact.detail<QContactOnlineAccount>().value("AccountPath"), QString(ACCOUNT_PATH));

    if (flags & Alias) {
        QString actualAlias = contact.detail<QContactDisplayLabel>().label();
        QCOMPARE(actualAlias, alias);
    }

    if (flags & Presence) {
        QContactPresence::PresenceState actualPresence = contact.detail<QContactPresence>().presenceState();
        switch (presence) {
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE:
            QCOMPARE(actualPresence, QContactPresence::PresenceAvailable);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY:
            QCOMPARE(actualPresence, QContactPresence::PresenceBusy);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY:
            QCOMPARE(actualPresence, QContactPresence::PresenceAway);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_OFFLINE:
            QCOMPARE(actualPresence, QContactPresence::PresenceOffline);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN:
            QCOMPARE(actualPresence, QContactPresence::PresenceUnknown);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_ERROR:
            break;
        }
    }

    if (flags & Avatar) {
        QString avatarFileName = contact.detail<QContactAvatar>().imageUrl().path();
        if (avatarData.isEmpty()) {
            QVERIFY(avatarFileName.isEmpty());
        } else {
            QFile file(avatarFileName);
            file.open(QIODevice::ReadOnly);
            QCOMPARE(file.readAll(), avatarData);
            file.close();
        }
    }

    if (flags & Authorization) {
        QContactPresence presence = contact.detail<QContactPresence>();
        QCOMPARE(presence.value("AuthStatusFrom"), subscriptionState);
        QCOMPARE(presence.value("AuthStatusTo"), publishState);
    }
}

TestTelepathyPlugin::TestTelepathyPlugin(QObject *parent) : Test(parent), mContactManager(0)
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
    mContactManager = new QContactManager(QLatin1String("tracker"));
    connect(mContactManager,
            SIGNAL(contactsAdded(const QList<QContactLocalId>&)),
            SLOT(contactsAdded(const QList<QContactLocalId>&)));
    connect(mContactManager,
            SIGNAL(contactsChanged(const QList<QContactLocalId>&)),
            SLOT(contactsChanged(const QList<QContactLocalId>&)));

    /* Create a fake Connection */
    mConnService = TP_BASE_CONNECTION(g_object_new(
            TP_TESTS_TYPE_CONTACTS_CONNECTION,
            "account", "fakeaccount",
            "protocol", "fakeproto",
            NULL));
    QVERIFY(mConnService != 0);
    QVERIFY(tp_base_connection_register(mConnService, "fakecm", NULL, NULL, NULL));
    TpHandleRepoIface *serviceRepo = tp_base_connection_get_handles(
        mConnService, TP_HANDLE_TYPE_CONTACT);
    mConnService->self_handle = tp_handle_ensure(serviceRepo,
        "fakeaccountid", NULL, NULL);
    tp_base_connection_change_status(mConnService,
        TP_CONNECTION_STATUS_CONNECTED,
        TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED);
    mListManager = tp_tests_contacts_connection_get_contact_list_manager(
        TP_TESTS_CONTACTS_CONNECTION(mConnService));

    /* Request the UnitTest bus name, so the AM knows we are ready to go */
    TpDBusDaemon *dbus = tp_dbus_daemon_dup(NULL);
    QVERIFY(tp_dbus_daemon_request_name(dbus, BUS_NAME, FALSE, NULL));
    g_object_unref (dbus);
}

void TestTelepathyPlugin::testBasicUpdates()
{
    /* Create a new contact "alice" */
    TpHandleRepoIface *serviceRepo =
        tp_base_connection_get_handles(mConnService, TP_HANDLE_TYPE_CONTACT);
    TpHandle handle = tp_handle_ensure(serviceRepo, "alice", NULL, NULL);
    QVERIFY(handle != 0);

    /* Set alias to "Alice" */
    const char *alias = "Alice";
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &alias);

    /* Add Alice in the ContactList */
    test_contact_list_manager_add_to_list(mListManager, NULL,
        TEST_CONTACT_LIST_SUBSCRIBE, handle, "wait", NULL);

    TestExpectation e;
    e.event = TestExpectation::Added;
    e.accountUri = QString("alice");
    e.alias = QString("Alice");
    e.presence = TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN;
    e.subscriptionState = "Requested";
    e.publishState = "No";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    /* Change the presence of Alice to busy */
    TpTestsContactsConnectionPresenceStatusIndex presence =
        TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY;
    const gchar *message = "Making coffee";
    tp_tests_contacts_connection_change_presences(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &presence, &message);

    e.event = TestExpectation::Changed;
    e.presence = presence;
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    /* Change the avatar of Alice */
    const gchar avatarData[] = "fake-avatar-data";
    const gchar avatarToken[] = "fake-avatar-token";
    const gchar avatarMimeType[] = "fake-avatar-mime-type";
    GArray *array = g_array_new(FALSE, FALSE, sizeof(gchar));
    g_array_append_vals (array, avatarData, strlen(avatarData));
    tp_tests_contacts_connection_change_avatar_data(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        handle, array, avatarMimeType, avatarToken);

    e.avatarData = QByteArray(avatarData);
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    qDebug() << "All expectations passed";
}

void TestTelepathyPlugin::testAuthorization()
{
    /* Create a new contact "romeo" */
    TpHandleRepoIface *serviceRepo =
        tp_base_connection_get_handles(mConnService, TP_HANDLE_TYPE_CONTACT);
    TpHandle handle = tp_handle_ensure(serviceRepo, "romeo", NULL, NULL);
    QVERIFY(handle != 0);

    /* Add Bob in the ContactList, the request will be ignored */
    test_contact_list_manager_add_to_list(mListManager, NULL,
        TEST_CONTACT_LIST_SUBSCRIBE, handle, "wait", NULL);

    TestExpectation e;
    e.flags = TestExpectation::Authorization;
    e.event = TestExpectation::Added;
    e.accountUri = QString("romeo");
    e.subscriptionState = "Requested";
    e.publishState = "No";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    /* Ask again for subscription, say "please" this time so it gets accepted */
    test_contact_list_manager_add_to_list(mListManager, NULL,
        TEST_CONTACT_LIST_SUBSCRIBE, handle, "please", NULL);

    e.event = TestExpectation::Changed;
    e.subscriptionState = "Yes";
    e.publishState = "Requested";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    handle = tp_handle_ensure(serviceRepo, "juliette", NULL, NULL);
    QVERIFY(handle != 0);

    /* Add Bob in the ContactList, the request will be ignored */
    test_contact_list_manager_add_to_list(mListManager, NULL,
        TEST_CONTACT_LIST_SUBSCRIBE, handle, "wait", NULL);

    e.event = TestExpectation::Added;
    e.accountUri = QString("juliette");
    e.subscriptionState = "Requested";
    e.publishState = "No";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    /* Ask again for subscription, but this time it will be rejected */
    test_contact_list_manager_add_to_list(mListManager, NULL,
        TEST_CONTACT_LIST_SUBSCRIBE, handle, "no", NULL);

    e.event = TestExpectation::Changed;
    e.subscriptionState = "No";
    e.publishState = "No";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);
}

void TestTelepathyPlugin::testSelfContact()
{
    QContactLocalId selfId = mContactManager->selfContactId();
    QContact contact = mContactManager->contact(selfId);
    qDebug() << contact;

    TestExpectation e;
    e.flags = TestExpectation::VerifyFlags(TestExpectation::Presence | TestExpectation::Avatar);
    e.accountUri = QString("fake@account.org");
    e.presence = TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE;
    e.verify(contact);
}

void TestTelepathyPlugin::testSetOffline()
{
    tp_base_connection_change_status(mConnService,
        TP_CONNECTION_STATUS_DISCONNECTED,
        TP_CONNECTION_STATUS_REASON_REQUESTED);

    TestExpectation e;
    e.flags = TestExpectation::Presence;
    e.event = TestExpectation::Changed;
    e.presence = TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN;
    e.accountUri = QString("romeo");
    mExpectations.append(e);
    e.accountUri = QString("alice");
    mExpectations.append(e);
    e.accountUri = QString("juliette");
    mExpectations.append(e);

    QCOMPARE(mLoop->exec(), 0);
}

void TestTelepathyPlugin::verify(TestExpectation::Event event,
    const QList<QContactLocalId> &contactIds)
{
    Q_FOREACH (QContactLocalId localId, contactIds) {
        QContactLocalId SelfContactId = 0x7FFFFFFF;
        if (localId == SelfContactId)
            continue;

        QContact contact = mContactManager->contact(localId);
        qDebug() << contact;

        if (mExpectations.isEmpty()) {
            mLoop->exit(0);
        }
        QVERIFY(!mExpectations.isEmpty());

        const TestExpectation &e = mExpectations.takeFirst();

        /* If we took the last expectation, quit the mainloop after a short
         * timeout. This is to make sure we don't get extra unwanted signals */
        if (mExpectations.isEmpty()) {
            QTimer::singleShot(500, mLoop, SLOT(quit()));
        }

        QCOMPARE(e.event, event);

        e.verify(contact);
    }
}

void TestTelepathyPlugin::contactsAdded(const QList<QContactLocalId>& contactIds)
{
    qDebug() << "Got contactsAdded";
    verify(TestExpectation::Added, contactIds);
}

void TestTelepathyPlugin::contactsChanged(const QList<QContactLocalId>& contactIds)
{
    qDebug() << "Got contactsChanged";
    verify(TestExpectation::Changed, contactIds);
}

void TestTelepathyPlugin::cleanupTestCase()
{
    tp_base_connection_change_status(mConnService,
        TP_CONNECTION_STATUS_DISCONNECTED,
        TP_CONNECTION_STATUS_REASON_REQUESTED);

    g_object_unref(mConnService);

    delete mContactManager;

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
