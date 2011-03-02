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

#include <QContact>
#include <QContactFetchByIdRequest>

#include <TelepathyQt4/Debug>

#include "libtelepathy/util.h"
#include "libtelepathy/debug.h"

#include "test-telepathy-plugin.h"
#include "buddymanagementinterface.h"
#include "debug.h"

TestTelepathyPlugin::TestTelepathyPlugin(QObject *parent) : Test(parent),
        mContactCount(0), mExpectation(0)
{
}

void TestTelepathyPlugin::initTestCase()
{
    initTestCaseImpl();

    qRegisterMetaType<QContactAbstractRequest::State>("QContactAbstractRequest::State");

    g_type_init();
    g_set_prgname("test-telepathy-plugin");

    if (!qgetenv("CONTACTSD_DEBUG").isEmpty()) {
        tp_debug_set_flags("all");
        test_debug_enable(TRUE);
        enableDebug(true);
    }

    dbus_g_bus_get(DBUS_BUS_STARTER, 0);

    /* Create a QContactManager and track added/changed contacts */
    mContactManager = new QContactManager(QLatin1String("tracker"));
    connect(mContactManager,
            SIGNAL(contactsAdded(const QList<QContactLocalId>&)),
            SLOT(contactsAdded(const QList<QContactLocalId>&)));
    connect(mContactManager,
            SIGNAL(contactsChanged(const QList<QContactLocalId>&)),
            SLOT(contactsChanged(const QList<QContactLocalId>&)));
    connect(mContactManager,
            SIGNAL(contactsRemoved(const QList<QContactLocalId>&)),
            SLOT(contactsRemoved(const QList<QContactLocalId>&)));

    /* Create a fake AccountManager */
    TpDBusDaemon *dbus = tp_dbus_daemon_dup(NULL);
    mAccountManager = (TpTestsSimpleAccountManager *) tp_tests_object_new_static_class(
            TP_TESTS_TYPE_SIMPLE_ACCOUNT_MANAGER, NULL);
    tp_dbus_daemon_register_object(dbus, TP_ACCOUNT_MANAGER_OBJECT_PATH, mAccountManager);
    tp_dbus_daemon_request_name (dbus, TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, NULL);
    g_object_unref(dbus);
}

void TestTelepathyPlugin::cleanupTestCase()
{
    cleanupTestCaseImpl();

    delete mContactManager;
    g_object_unref(mAccountManager);
}

void TestTelepathyPlugin::init()
{
    initImpl();

    /* Create a fake Connection */
    tp_tests_create_and_connect_conn(TP_TESTS_TYPE_CONTACTS_CONNECTION,
            "fakeaccount", &mConnService, &mConnection);
    QVERIFY(mConnService);
    QVERIFY(mConnection);

    mListManager = tp_tests_contacts_connection_get_contact_list_manager(
        TP_TESTS_CONTACTS_CONNECTION(mConnService));

    /* Define the self contact */
    TpTestsContactsConnectionPresenceStatusIndex presence =
            TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE;
    const gchar *message = "Running unit tests";
    tp_tests_contacts_connection_change_presences(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &mConnService->self_handle, &presence, &message);
    const gchar *aliases = "badger";
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &mConnService->self_handle, &aliases);

    /* Create a fake Account */
    TpDBusDaemon *dbus = tp_dbus_daemon_dup(NULL);
    mAccount = (TpTestsSimpleAccount *) tp_tests_object_new_static_class(
            TP_TESTS_TYPE_SIMPLE_ACCOUNT, NULL);
    tp_dbus_daemon_register_object(dbus, ACCOUNT_PATH, mAccount);
    tp_tests_simple_account_manager_add_account (mAccountManager, ACCOUNT_PATH, TRUE);
    tp_tests_simple_account_set_connection (mAccount, mConnService->object_path);
    g_object_unref(dbus);

    /* Wait for self contact to appear */
    TestExpectationInit exp;
    runExpectation(&exp);
    mContactCount = 1;
}

void TestTelepathyPlugin::cleanup()
{
    cleanupImpl();

    tp_cli_connection_call_disconnect(mConnection, -1, NULL, NULL, NULL, NULL);
    tp_tests_simple_account_manager_remove_account(mAccountManager, ACCOUNT_PATH);
    tp_tests_simple_account_removed(mAccount);

    /* Wait for all contacts to disappear */
    TestExpectationCleanup exp(mContactCount);
    runExpectation(&exp);

    g_object_unref(mConnService);
    g_object_unref(mConnection);
    g_object_unref(mAccount);
}

void TestTelepathyPlugin::testBasicUpdates()
{
    /* Create a new contact */
    TpHandle handle = ensureContact("testbasicupdates");

    /* Set alias to "Alice" */
    const char *alias = "Alice";
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &alias);

    /* Add it in the ContactList */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("testbasicupdates");
    exp.verifyAlias("Alice");
    exp.verifyPresence(TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN);
    exp.verifyAuthorization("Requested", "No");
    runExpectation(&exp);

    /* Change the presence to busy */
    TpTestsContactsConnectionPresenceStatusIndex presence =
            TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY;
    const gchar *message = "Making coffee";
    tp_tests_contacts_connection_change_presences(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &presence, &message);

    exp.setEvent(EventChanged);
    exp.verifyPresence(presence);
    runExpectation(&exp);
}

void TestTelepathyPlugin::testSelfContact()
{
    TestExpectationContact exp(EventChanged);
    exp.verifyPresence(TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE);
    exp.verifyAvatar(QByteArray());
    exp.verifyOnlineAccount("fakeaccount");

    // Simulate a change in self contact
    verify(EventChanged, QList<QContactLocalId>() << mContactManager->selfContactId());

    runExpectation(&exp);
}

void TestTelepathyPlugin::testAuthorization()
{
    /* Create a new contact "romeo" */
    TpHandle handle = ensureContact("romeo");

    /* Add him in the ContactList, the request will be ignored */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("romeo");
    exp.verifyAuthorization("Requested", "No");
    runExpectation(&exp);

    /* Ask again for subscription, say "please" this time so it gets accepted */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    exp.setEvent(EventChanged);
    exp.verifyAuthorization("Yes", "Requested");
    runExpectation(&exp);

    handle = ensureContact("juliette");

    /* Add her in the ContactList, the request will be ignored */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    exp.setEvent(EventAdded);
    exp.verifyOnlineAccount("juliette");
    exp.verifyAuthorization("Requested", "No");
    runExpectation(&exp);

    /* Ask again for subscription, but this time it will be rejected */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "no");

    exp.setEvent(EventChanged);
    exp.verifyAuthorization("No", "No");
    runExpectation(&exp);
}

GPtrArray *TestTelepathyPlugin::createContactInfoTel(const gchar *number)
{
    GPtrArray *infoPtrArray;

    infoPtrArray = g_ptr_array_new_with_free_func((GDestroyNotify) g_value_array_free);
    const gchar *fieldValues[] = { number, NULL };
    g_ptr_array_add (infoPtrArray, tp_value_array_build(3,
        G_TYPE_STRING, "tel",
        G_TYPE_STRV, NULL,
        G_TYPE_STRV, fieldValues,
        G_TYPE_INVALID));

    return infoPtrArray;
}

void TestTelepathyPlugin::testContactInfo()
{
    /* Create a contact with no ContactInfo */
    TpHandle handle = ensureContact("testcontactinfo");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("testcontactinfo");
    runExpectation(&exp);

    /* Set some ContactInfo on the contact */
    GPtrArray *infoPtrArray = createContactInfoTel("123");
    const gchar *fieldValues[] = { "email@foo.com", NULL };
    g_ptr_array_add (infoPtrArray, tp_value_array_build(3,
        G_TYPE_STRING, "email",
        G_TYPE_STRV, NULL,
        G_TYPE_STRV, fieldValues,
        G_TYPE_INVALID));
    tp_tests_contacts_connection_change_contact_info(
        TP_TESTS_CONTACTS_CONNECTION(mConnService), handle, infoPtrArray);

    exp.setEvent(EventChanged);
    exp.verifyInfo(infoPtrArray);
    runExpectation(&exp);
    g_ptr_array_unref(infoPtrArray);

    /* Change the ContactInfo */
    infoPtrArray = createContactInfoTel("456");
    tp_tests_contacts_connection_change_contact_info(
        TP_TESTS_CONTACTS_CONNECTION(mConnService), handle, infoPtrArray);

    exp.verifyInfo(infoPtrArray);
    runExpectation(&exp);
    g_ptr_array_unref(infoPtrArray);
}

void TestTelepathyPlugin::testBug220851()
{
    /* Create a contact with no ContactInfo */
    TpHandle handle = ensureContact("testbug220851");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("testbug220851");
    runExpectation(&exp);

    /* An address has 7 fields normally. Verify it's fine to give less */
    GPtrArray *infoPtrArray = g_ptr_array_new_with_free_func((GDestroyNotify) g_value_array_free);
    const gchar *fieldValues[] = { "pobox", "extendedaddress", "street", NULL };
    g_ptr_array_add (infoPtrArray, tp_value_array_build(3,
        G_TYPE_STRING, "adr",
        G_TYPE_STRV, NULL,
        G_TYPE_STRV, fieldValues,
        G_TYPE_INVALID));

    tp_tests_contacts_connection_change_contact_info(
        TP_TESTS_CONTACTS_CONNECTION(mConnService), handle, infoPtrArray);

    exp.setEvent(EventChanged);
    exp.verifyInfo(infoPtrArray);
    runExpectation(&exp);
    g_ptr_array_unref(infoPtrArray);
}

void TestTelepathyPlugin::testRemoveContacts()
{
    TpHandle handle = ensureContact("testremovecontacts");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("testremovecontacts");
    runExpectation(&exp);

    test_contact_list_manager_remove(mListManager, 1, &handle);
    exp.setEvent(EventRemoved);
    runExpectation(&exp);
}

#if 0
void TestTelepathyPlugin::testRemoveBuddyDBusAPI()
{
    // add again and remove using RemoveBuddy
    TpHandle  handle = ensureContact("buddyyes");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    mExpectation.event = TestExpectation::Added;
    mExpectation.flags = TestExpectation::OnlineAccounts;
    mExpectation.fetchError = QContactManager::NoError;
    mExpectation.accountUri = "buddyyes";
    QCOMPARE(mLoop->exec(), 0);

    handle = ensureContact("buddyNot");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    QString id2Remove("buddynot");
    mExpectation.event = TestExpectation::Added;
    mExpectation.flags = TestExpectation::OnlineAccounts;
    mExpectation.accountUri = id2Remove;
    QCOMPARE(mLoop->exec(), 0);

    // remove buddynot and keep buddyyes
    BuddyManagementInterface *buddyIf = new BuddyManagementInterface("com.nokia.contactsd", "/telepathy", QDBusConnection::sessionBus(), 0);
    {
        QDBusPendingReply<> async = buddyIf->removeBuddy(ACCOUNT_PATH, id2Remove);
        QDBusPendingCallWatcher watcher(async, this);
        watcher.waitForFinished();
        QVERIFY2(not async.isError(), async.error().message().toLatin1());
        QVERIFY(watcher.isValid());
    }
    mExpectation.event = TestExpectation::Removed;
    mExpectation.fetchError = QContactManager::DoesNotExistError;
    QCOMPARE(mLoop->exec(), 0);

    // disconnect and try to remove buddyyes - buddyyes should stay in telepathycontact manager
    // but removed from tracker
    tp_cli_connection_call_disconnect(mConnection, -1, NULL, NULL, NULL, NULL);
    mState = TestStateDisconnecting;
    mExpectation.fetchError = QContactManager::NoError;
    QCOMPARE(mLoop->exec(), 0);
    {
        QDBusPendingReply<> async = buddyIf->removeBuddy(ACCOUNT_PATH, "buddyyes");
        QDBusPendingCallWatcher watcher(async, this);
        watcher.waitForFinished();
        QVERIFY2(not async.isError(), async.error().message().toLatin1());
        QVERIFY(watcher.isValid());

    }

    // FIXME - connect back and verify that the contact has been removed from test contact manager
    //QSKIP(QTest::SkipAll, "This part is yet implemented");
    mExpectation.event = TestExpectation::Removed;
    mState = TestStateReady;
    mExpectation.fetchError = QContactManager::DoesNotExistError;
    mExpectation.accountUri = "buddyyes";
    QCOMPARE(mLoop->exec(), 0);
}
#endif

void TestTelepathyPlugin::testInviteBuddyDBusAPI()
{
    const QString buddy("buddy");
    // remove buddynot and keep buddyyes
    BuddyManagementInterface *buddyIf = new BuddyManagementInterface("com.nokia.contactsd", "/telepathy", QDBusConnection::sessionBus(), 0);
    {
        QDBusPendingReply<> async = buddyIf->inviteBuddy(ACCOUNT_PATH, buddy);
        QDBusPendingCallWatcher watcher(async, this);
        watcher.waitForFinished();
        QVERIFY2(not async.isError(), async.error().message().toLatin1());
        QVERIFY(watcher.isValid());
    }

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount(buddy);
    runExpectation(&exp);
}

void TestTelepathyPlugin::testSetOffline()
{
    TpHandle handle = ensureContact("testsetoffline");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("testsetoffline");
    runExpectation(&exp);

    tp_cli_connection_call_disconnect(mConnection, -1, NULL, NULL, NULL, NULL);

    TestExpectationDisconnect exp2(mContactCount);
    runExpectation(&exp2);
}

void TestTelepathyPlugin::testAvatar()
{
    const gchar avatarData[] = "fake-avatar-data";
    const gchar avatarToken[] = "fake-avatar-token";
    const gchar avatarMimeType[] = "fake-avatar-mime-type";

    /* Create a contact with an avatar  */
    TpHandle handle = ensureContact("testavatar");
    GArray *array = g_array_new(FALSE, FALSE, sizeof(gchar));
    g_array_append_vals(array, avatarData, strlen(avatarData));
    tp_tests_contacts_connection_change_avatar_data(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        handle, array, avatarMimeType, avatarToken);
    g_array_unref(array);

    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("testavatar");
    exp.verifyAvatar(QByteArray(avatarData));
    runExpectation(&exp);

    /* Change avatar */
    const gchar avatarData2[] = "fake-avatar-data-2";
    const gchar avatarToken2[] = "fake-avatar-token-2";
    const gchar avatarMimeType2[] = "fake-avatar-mime-type-2";
    array = g_array_new(FALSE, FALSE, sizeof(gchar));
    g_array_append_vals(array, avatarData2, strlen(avatarData2));
    tp_tests_contacts_connection_change_avatar_data(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        handle, array, avatarMimeType2, avatarToken2);
    g_array_unref(array);

    exp.setEvent(EventChanged);
    exp.verifyAvatar(QByteArray(avatarData2));
    runExpectation(&exp);

    /* Set back initial avatar */
    array = g_array_new(FALSE, FALSE, sizeof(gchar));
    g_array_append_vals(array, avatarData, strlen(avatarData));
    tp_tests_contacts_connection_change_avatar_data(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        handle, array, avatarMimeType, avatarToken);
    g_array_unref(array);

    exp.verifyAvatar(QByteArray(avatarData));
    runExpectation(&exp);

    /* Create another contact with the same avatar, they will share the same
     * nfo:FileObjectData in tracker */
    TpHandle handle2 = ensureContact("testavatar2");
    array = g_array_new(FALSE, FALSE, sizeof(gchar));
    g_array_append_vals(array, avatarData, strlen(avatarData));
    tp_tests_contacts_connection_change_avatar_data(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        handle2, array, avatarMimeType, avatarToken);
    g_array_unref(array);

    test_contact_list_manager_request_subscription(mListManager, 1, &handle2,
        "please");

    TestExpectationContact exp2(EventAdded);
    exp2.verifyOnlineAccount("testavatar2");
    exp2.verifyAvatar(QByteArray(avatarData));
    runExpectation(&exp2);

    /* Change avatar of the new contact */
    array = g_array_new(FALSE, FALSE, sizeof(gchar));
    g_array_append_vals(array, avatarData2, strlen(avatarData2));
    tp_tests_contacts_connection_change_avatar_data(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        handle2, array, avatarMimeType2, avatarToken2);
    g_array_unref(array);

    exp2.setEvent(EventChanged);
    exp2.verifyAvatar(QByteArray(avatarData2));
    runExpectation(&exp2);

    /* Change the alias of first contact, this is to force fetching again
     * the contact, so verify the shared nfo:FileObjectData for the avatar is
     * still there */
    const char *alias = "Where is my Avatar ?";
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &alias);

    exp.verifyAlias(alias);
    runExpectation(&exp);
}

void TestTelepathyPlugin::testIRIEncode()
{
    /* Create a contact with a special id that could confuse tracker */
    TpHandle handle = ensureContact("<specialid>");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    TestExpectationContact exp(EventAdded);
    exp.verifyOnlineAccount("<specialid>");
    runExpectation(&exp);
}

TpHandle TestTelepathyPlugin::ensureContact(const gchar *id)
{
    TpHandleRepoIface *serviceRepo =
        tp_base_connection_get_handles(mConnService, TP_HANDLE_TYPE_CONTACT);

    TpHandle handle = tp_handle_ensure(serviceRepo, id, NULL, NULL);

    return handle;
}

void TestTelepathyPlugin::runExpectation(TestExpectation *exp)
{
    QVERIFY(mExpectation == 0);
    mExpectation = exp;
    mExpectation->setContactManager(mContactManager);
    connect(mExpectation, SIGNAL(finished()),
            mLoop, SLOT(quit()));

    QCOMPARE(mLoop->exec(), 0);

    mExpectation = 0;
}

void TestTelepathyPlugin::contactsAdded(const QList<QContactLocalId>& contactIds)
{
    debug() << "Got contactsAdded";
    mContactCount += contactIds.count();
    verify(EventAdded, contactIds);
}

void TestTelepathyPlugin::contactsChanged(const QList<QContactLocalId>& contactIds)
{
    debug() << "Got contactsChanged";
    verify(EventChanged, contactIds);
}

void TestTelepathyPlugin::contactsRemoved(const QList<QContactLocalId>& contactIds)
{
    debug() << "Got contactsRemoved";
    mContactCount -= contactIds.count();
    QVERIFY(mContactCount >= 0);
    verify(EventRemoved, contactIds);
}

void TestTelepathyPlugin::verify(Event event,
    const QList<QContactLocalId> &contactIds)
{
    new TestFetchContacts(this, event, contactIds);
}

TestFetchContacts::TestFetchContacts(TestTelepathyPlugin *parent, Event event,
        const QList<QContactLocalId> &contactIds)
    : QObject(parent), mTest(parent), mEvent(event), mContactIds(contactIds)
{
    QContactFetchByIdRequest *request = new QContactFetchByIdRequest();
    connect(request, SIGNAL(resultsAvailable()),
        SLOT(onContactsFetched()));
    request->setManager(mTest->mContactManager);
    request->setLocalIds(contactIds);
    QVERIFY(request->start());
}

void TestFetchContacts::onContactsFetched()
{
    QContactFetchByIdRequest *req = qobject_cast<QContactFetchByIdRequest *>(sender());
    if (req == 0 || !req->isFinished()) {
        return;
    }

    QVERIFY(mTest->mExpectation != 0);
    if (req->error() == QContactManager::NoError) {
        mTest->mExpectation->verify(mEvent, req->contacts());
    } else {
        mTest->mExpectation->verify(mEvent, mContactIds, req->error());
    }

    deleteLater();
}

QTEST_MAIN(TestTelepathyPlugin)
