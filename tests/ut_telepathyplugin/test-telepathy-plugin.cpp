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
#include <QContactAvatar>
#include <QContactFetchByIdRequest>
#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactAddress>
#include <QContactPresence>

#include <TelepathyQt4/Debug>

#include "libtelepathy/util.h"
#include "libtelepathy/debug.h"

#include "test-telepathy-plugin.h"
#include "buddymanagementinterface.h"

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "fakecm/fakeproto/fakeaccount"

bool debugEnabled = false;

struct DiscardDevice : public QIODevice
{
    qint64 readData(char* data, qint64 maxSize)
    {
        Q_UNUSED(data);
        Q_UNUSED(maxSize);
        return 0;
    }

    qint64 writeData(const char* data, qint64 maxsize)
    {
        Q_UNUSED(data);
        return maxsize;
    }
} discard;

QDebug enabledDebug()
{
    if (debugEnabled) {
        return qDebug();
    } else {
        return QDebug(&discard);
    }
}

inline QDebug debug()
{
    return enabledDebug() << "DEBUG:";
}

TestTelepathyPlugin::TestTelepathyPlugin(QObject *parent) : Test(parent),
        mState(TestStateNone), mContactCount(0)
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
        debugEnabled = true;
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

    mState = TestStateInit;

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

    /* Wait to be ready (self contact appear) */
    QCOMPARE(mLoop->exec(), 0);
    QCOMPARE(mState, TestStateReady);
}

void TestTelepathyPlugin::cleanup()
{
    cleanupImpl();

    mState = TestStateCleanup;

    tp_cli_connection_call_disconnect(mConnection, -1, NULL, NULL, NULL, NULL);
    tp_tests_simple_account_manager_remove_account(mAccountManager, ACCOUNT_PATH);
    tp_tests_simple_account_removed(mAccount);

    /* Wait to be not-ready (self contact disappear) */
    QCOMPARE(mLoop->exec(), 0);
    QCOMPARE(mState, TestStateNone);

    mExpectation = TestExpectation();
    g_object_unref(mConnService);
    g_object_unref(mConnection);
    g_object_unref(mAccount);
}

void TestTelepathyPlugin::testBasicUpdates()
{
    /* Create a new contact "alice" */
    TpHandle handle = ensureContact("alice");

    /* Set alias to "Alice" */
    const char *alias = "Alice";
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &alias);

    /* Add Alice in the ContactList */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    mExpectation.event = TestExpectation::Added;
    mExpectation.accountUri = QString("alice");
    mExpectation.alias = QString("Alice");
    mExpectation.presence = TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN;
    mExpectation.subscriptionState = "Requested";
    mExpectation.publishState = "No";
    QCOMPARE(mLoop->exec(), 0);

    /* Change the presence of Alice to busy */
    TpTestsContactsConnectionPresenceStatusIndex presence =
            TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY;
    const gchar *message = "Making coffee";
    tp_tests_contacts_connection_change_presences(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &presence, &message);

    mExpectation.event = TestExpectation::Changed;
    mExpectation.presence = presence;
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

    mExpectation.avatarData = QByteArray(avatarData);
    QCOMPARE(mLoop->exec(), 0);

    debug() << "All expectations passed";
}

void TestTelepathyPlugin::testSelfContact()
{
    mExpectation.flags = TestExpectation::Presence | TestExpectation::Avatar |
            TestExpectation::OnlineAccounts;
    mExpectation.accountUri = QString("fakeaccount");
    mExpectation.presence = TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE;

    fetchAndVerifyContacts(QList<QContactLocalId>() << mContactManager->selfContactId());

    QCOMPARE(mLoop->exec(), 0);
}

void TestTelepathyPlugin::testAuthorization()
{
    /* Create a new contact "romeo" */
    TpHandle handle = ensureContact("romeo");

    /* Add Bob in the ContactList, the request will be ignored */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    mExpectation.flags = TestExpectation::Authorization | TestExpectation::OnlineAccounts;
    mExpectation.event = TestExpectation::Added;
    mExpectation.accountUri = QString("romeo");
    mExpectation.subscriptionState = "Requested";
    mExpectation.publishState = "No";
    QCOMPARE(mLoop->exec(), 0);

    /* Ask again for subscription, say "please" this time so it gets accepted */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    mExpectation.event = TestExpectation::Changed;
    mExpectation.subscriptionState = "Yes";
    mExpectation.publishState = "Requested";
    QCOMPARE(mLoop->exec(), 0);

    handle = ensureContact("juliette");

    /* Add Bob in the ContactList, the request will be ignored */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    mExpectation.event = TestExpectation::Added;
    mExpectation.accountUri = QString("juliette");
    mExpectation.subscriptionState = "Requested";
    mExpectation.publishState = "No";
    QCOMPARE(mLoop->exec(), 0);

    /* Ask again for subscription, but this time it will be rejected */
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "no");

    mExpectation.event = TestExpectation::Changed;
    mExpectation.subscriptionState = "No";
    mExpectation.publishState = "No";
    QCOMPARE(mLoop->exec(), 0);
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
    TpHandle handle = ensureContact("skype");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    mExpectation.event = TestExpectation::Added;
    mExpectation.flags = TestExpectation::OnlineAccounts;
    mExpectation.accountUri = QString("skype");
    QCOMPARE(mLoop->exec(), 0);

    mExpectation.event = TestExpectation::Changed;
    mExpectation.flags = TestExpectation::Info | TestExpectation::OnlineAccounts;

    /* Set some ContactInfo on the contact */
    GPtrArray *infoPtrArray = createContactInfoTel("123");
    tp_tests_contacts_connection_change_contact_info(
        TP_TESTS_CONTACTS_CONNECTION(mConnService), handle, infoPtrArray);

    mExpectation.contactInfo = infoPtrArray;
    QCOMPARE(mLoop->exec(), 0);
    g_ptr_array_unref(infoPtrArray);

    /* Change the ContactInfo */
    infoPtrArray = createContactInfoTel("456");
    tp_tests_contacts_connection_change_contact_info(
        TP_TESTS_CONTACTS_CONNECTION(mConnService), handle, infoPtrArray);

    mExpectation.contactInfo = infoPtrArray;
    QCOMPARE(mLoop->exec(), 0);
    g_ptr_array_unref(infoPtrArray);
}

void TestTelepathyPlugin::testBug220851()
{
    /* Create a contact with no ContactInfo */
    TpHandle handle = ensureContact("bug220851");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "wait");

    mExpectation.event = TestExpectation::Added;
    mExpectation.flags = TestExpectation::OnlineAccounts;
    mExpectation.accountUri = QString("bug220851");
    QCOMPARE(mLoop->exec(), 0);

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

    mExpectation.event = TestExpectation::Changed;
    mExpectation.flags = TestExpectation::Info | TestExpectation::OnlineAccounts;
    mExpectation.contactInfo = infoPtrArray;
    QCOMPARE(mLoop->exec(), 0);
    g_ptr_array_unref(infoPtrArray);
}

void TestTelepathyPlugin::testRemoveContacts()
{
    TpHandle handle = ensureContact("plop");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    mExpectation.event = TestExpectation::Added;
    mExpectation.flags = TestExpectation::OnlineAccounts;
    mExpectation.accountUri = QString::fromLatin1("plop");
    QCOMPARE(mLoop->exec(), 0);

    test_contact_list_manager_remove(mListManager, 1, &handle);
    mExpectation.event = TestExpectation::Removed;
    mExpectation.fetchError = QContactManager::DoesNotExistError;
    QCOMPARE(mLoop->exec(), 0);
}

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
    mExpectation.event = TestExpectation::Added;
    mExpectation.flags = TestExpectation::OnlineAccounts;
    mExpectation.accountUri = buddy;
    QCOMPARE(mLoop->exec(), 0);
}

void TestTelepathyPlugin::testSetOffline()
{
    TpHandle handle = ensureContact("kesh");
    test_contact_list_manager_request_subscription(mListManager, 1, &handle,
        "please");

    mExpectation.flags = TestExpectation::OnlineAccounts;
    mExpectation.accountUri = QString("kesh");
    mExpectation.event = TestExpectation::Added;
    QCOMPARE(mLoop->exec(), 0);

    tp_cli_connection_call_disconnect(mConnection, -1, NULL, NULL, NULL, NULL);

    mState = TestStateDisconnecting;
    QCOMPARE(mLoop->exec(), 0);
}

TpHandle TestTelepathyPlugin::ensureContact(const gchar *id)
{
    TpHandleRepoIface *serviceRepo =
        tp_base_connection_get_handles(mConnService, TP_HANDLE_TYPE_CONTACT);

    TpHandle handle = tp_handle_ensure(serviceRepo, id, NULL, NULL);

    return handle;
}

void TestTelepathyPlugin::onContactsFetched()
{
    QContactFetchByIdRequest *req = qobject_cast<QContactFetchByIdRequest *>(sender());
    if (req == 0 || !req->isFinished()) {
        return;
    }

    QCOMPARE(req->error(), mExpectation.fetchError);
    if (req->error() == QContactManager::NoError) {
        if (mState != TestStateDisconnecting) {
            QCOMPARE(req->contacts().count(), 1);
            QContact contact = req->contacts().at(0);
            debug() << contact;
            mExpectation.verify(contact);
        } else {
            bool foundSelf = false;
            int contactCount = 0;
            mExpectation.flags = TestExpectation::Presence;
            Q_FOREACH (QContact contact, req->contacts()) {
                if (contact.localId() == mContactManager->selfContactId()) {
                    foundSelf = true;
                    mExpectation.presence = TP_TESTS_CONTACTS_CONNECTION_STATUS_OFFLINE;
                } else {
                    contactCount++;
                    mExpectation.presence = TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN;
                }
                debug() << contact;
                mExpectation.verify(contact);
            }
            QVERIFY2(foundSelf, "We were expecting self contact to be updated");
            QCOMPARE(contactCount, mContactCount - 1);
        }
    }

    mExpectation.fetching = false;
    req->deleteLater();
    mLoop->exit(0);
}

void TestTelepathyPlugin::fetchAndVerifyContacts(const QList<QContactLocalId> &contactIds)
{
    QVERIFY(!mExpectation.fetching);
    mExpectation.fetching = true;

    QContactFetchByIdRequest *request = new QContactFetchByIdRequest();
    connect(request, SIGNAL(resultsAvailable()),
        SLOT(onContactsFetched()));
    request->setManager(mContactManager);
    request->setLocalIds(contactIds);
    QVERIFY(request->start());
}

void TestTelepathyPlugin::verify(TestExpectation::Event event,
    const QList<QContactLocalId> &contactIds)
{
    /* If we are intializing, wait for self contact update */
    if (mState == TestStateInit) {
        QCOMPARE(event, TestExpectation::Changed);
        QCOMPARE(contactIds.count(), 1);
        QCOMPARE(contactIds[0], mContactManager->selfContactId());
        mContactCount++;
        mState = TestStateReady;
        mLoop->exit(0);
        return;
    }

    if (mState == TestStateDisconnecting) {
        QCOMPARE(event, TestExpectation::Changed);
        fetchAndVerifyContacts(contactIds);
        return;
    }

    /* If we are cleaning up, wait for all contacts to be removed */
    if (mState == TestStateCleanup) {
        if (event == TestExpectation::Changed) {
            QCOMPARE(contactIds.count(), 1);
            QCOMPARE(contactIds[0], mContactManager->selfContactId());
            mContactCount--;
        }
        if (mContactCount == 0) {
            mState = TestStateNone;
            mLoop->exit(0);
        }
        return;
    }


    QCOMPARE(mState, TestStateReady);
    QCOMPARE(event, mExpectation.event);
    QCOMPARE(contactIds.count(), 1);
    fetchAndVerifyContacts(contactIds);
}

void TestTelepathyPlugin::contactsAdded(const QList<QContactLocalId>& contactIds)
{
    debug() << "Got contactsAdded";
    mContactCount += contactIds.count();
    verify(TestExpectation::Added, contactIds);
}

void TestTelepathyPlugin::contactsChanged(const QList<QContactLocalId>& contactIds)
{
    debug() << "Got contactsChanged";
    verify(TestExpectation::Changed, contactIds);
}

void TestTelepathyPlugin::contactsRemoved(const QList<QContactLocalId>& contactIds)
{
    debug() << "Got contactsRemoved";
    mContactCount -= contactIds.count();
    QVERIFY(mContactCount >= 0);
    verify(TestExpectation::Removed, contactIds);
}

TestExpectation::TestExpectation():flags(All), nOnlineAccounts(1),
    presence(TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN), contactInfo(NULL),
    fetching(false), fetchError(QContactManager::NoError)
{
}

void TestExpectation::verify(QContact &contact) const
{
    if (flags & OnlineAccounts) {
        QCOMPARE(contact.details<QContactOnlineAccount>().count(), nOnlineAccounts);
        QCOMPARE(contact.details<QContactPresence>().count(), nOnlineAccounts);
        if (nOnlineAccounts == 1) {
            QCOMPARE(contact.detail<QContactOnlineAccount>().accountUri(), accountUri);
            QCOMPARE(contact.detail<QContactOnlineAccount>().value("AccountPath"), QString(ACCOUNT_PATH));

        }
    }

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
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_ERROR:
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_UNSET:
            QCOMPARE(actualPresence, QContactPresence::PresenceUnknown);
            break;
        }
    }

    if (flags & Avatar) {
        QString avatarFileName = contact.detail<QContactAvatar>().imageUrl().path();
        if (avatarData.isEmpty()) {
            QVERIFY2(avatarFileName.isEmpty(), "Expected empty avatar filename");
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

    if (flags & Info) {
        uint nMatchedField = 0;

        Q_FOREACH (const QContactDetail &detail, contact.details()) {
            if (detail.definitionName() == "PhoneNumber") {
                QContactPhoneNumber phoneNumber = static_cast<QContactPhoneNumber>(detail);
                verifyContactInfo(contactInfo, "tel", QStringList() << phoneNumber.number());
                nMatchedField++;
            }
            else if (detail.definitionName() == "Address") {
                QContactAddress address = static_cast<QContactAddress>(detail);
                verifyContactInfo(contactInfo, "adr",
                        QStringList() << address.postOfficeBox()
                                      << QString("unmapped") // extended address is not mapped
                                      << address.street()
                                      << address.locality()
                                      << address.region()
                                      << address.postcode()
                                      << address.country());
                nMatchedField++;
            }
        }

        if (contactInfo != NULL) {
            QCOMPARE(nMatchedField, contactInfo->len);
        }
    }
}

void TestExpectation::verifyContactInfo(GPtrArray *infoPtrArray, QString name,
        const QStringList values) const
{
    QVERIFY2(infoPtrArray != NULL, "Found ContactInfo field, was expecting none");

    bool found = false;
    for (uint i = 0; i < contactInfo->len; i++) {
        gchar *c_name;
        gchar **c_parameters;
        gchar **c_values;

        tp_value_array_unpack((GValueArray*) g_ptr_array_index(contactInfo, i),
            3, &c_name, &c_parameters, &c_values);

        /* if c_values_len < values.count() it could still be OK if all
         * additional values are empty. */
        gint c_values_len = g_strv_length(c_values);
        if (QString(c_name) != name || c_values_len > values.count()) {
            continue;
        }

        bool match = true;
        for (int j = 0; j < values.count(); j++) {
            if (values[j] == QString("unmapped")) {
                continue;
            }
            if ((j < c_values_len && values[j] != QString(c_values[j])) ||
                (j >= c_values_len && !values[j].isEmpty())) {
                match = false;
                break;
            }
        }

        if (match) {
            found = true;
            break;
        }
    }

    QVERIFY2(found, "Unexpected ContactInfo field");
}

QTEST_MAIN(TestTelepathyPlugin)
