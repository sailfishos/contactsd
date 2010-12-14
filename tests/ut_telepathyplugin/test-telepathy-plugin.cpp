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

TestExpectation::TestExpectation():flags(All), nOnlineAccounts(1),
    presence(TP_CONNECTION_PRESENCE_TYPE_UNKNOWN)
{
}

void TestExpectation::verify(QContact &contact) const
{
    QCOMPARE(contact.details<QContactOnlineAccount>().count(), nOnlineAccounts);
    QCOMPARE(contact.details<QContactPresence>().count(), nOnlineAccounts);
    if (nOnlineAccounts == 1) {
        QCOMPARE(contact.detail<QContactOnlineAccount>().accountUri(), accountUri);
        QCOMPARE(contact.detail<QContactOnlineAccount>().value("AccountPath"), QString(ACCOUNT_PATH));
    }

    if (flags & Alias) {
        QString actualAlias = contact.detail<QContactDisplayLabel>().label();
        QCOMPARE(actualAlias, alias);
    }

    if (flags & Presence) {
        QContactPresence::PresenceState actualPresence = contact.detail<QContactPresence>().presenceState();
        switch (presence) {
        case TP_CONNECTION_PRESENCE_TYPE_UNSET:
            QCOMPARE(actualPresence, QContactPresence::PresenceUnknown);
            break;
        case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
            QCOMPARE(actualPresence, QContactPresence::PresenceOffline);
            break;
        case TP_CONNECTION_PRESENCE_TYPE_AVAILABLE:
            QCOMPARE(actualPresence, QContactPresence::PresenceAvailable);
            break;
        case TP_CONNECTION_PRESENCE_TYPE_AWAY:
        case TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY:
            QCOMPARE(actualPresence, QContactPresence::PresenceAway);
            break;
        case TP_CONNECTION_PRESENCE_TYPE_HIDDEN:
            QCOMPARE(actualPresence, QContactPresence::PresenceHidden);
            break;
        case TP_CONNECTION_PRESENCE_TYPE_BUSY:
            QCOMPARE(actualPresence, QContactPresence::PresenceBusy);
            break;
        case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
        case TP_CONNECTION_PRESENCE_TYPE_ERROR:
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
        QList<QContactDetail> contactDetails;

        /* Keep only the details we care about */
        Q_FOREACH (const QContactDetail &detail, contact.details()) {
            if (detail.definitionName() == "PhoneNumber") {
                contactDetails << detail;
            }
        }

        Q_FOREACH (const QContactDetail &detail, details) {
            bool matched = false;
            for (int i = 0; i < contactDetails.size(); i++) {
                if (detail == contactDetails[i]) {
                    matched = true;
                    contactDetails.removeAt(i);
                    break;
                }
            }
            QVERIFY2(matched, "Expected detail not found");
        }
        QVERIFY2(contactDetails.isEmpty(), "Detail not expected");
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
    connect(mContactManager,
            SIGNAL(contactsRemoved(const QList<QContactLocalId>&)),
            SLOT(contactsRemoved(const QList<QContactLocalId>&)));

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
        "fake@account.org", NULL, NULL);
    tp_base_connection_change_status(mConnService,
        TP_CONNECTION_STATUS_CONNECTED,
        TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED);
    mListManager = tp_tests_contacts_connection_get_contact_list_manager(
        TP_TESTS_CONTACTS_CONNECTION(mConnService));

    /* Define the self contact */
    TpConnectionPresenceType presence = TP_CONNECTION_PRESENCE_TYPE_AVAILABLE;
    const gchar *message = "Running unit tests";
    tp_tests_contacts_connection_change_presences(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &mConnService->self_handle, &presence, &message);
    const gchar *aliases = "badger";
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &mConnService->self_handle, &aliases);

    /* Request the UnitTest bus name, so the AM knows we are ready to go */
    TpDBusDaemon *dbus = tp_dbus_daemon_dup(NULL);
    QVERIFY(tp_dbus_daemon_request_name(dbus, BUS_NAME, FALSE, NULL));
    g_object_unref (dbus);
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
    test_contact_list_manager_request_subscription(mListManager, handle, "wait");

    TestExpectation e;
    e.event = TestExpectation::Added;
    e.accountUri = QString("alice");
    e.alias = QString("Alice");
    e.presence = TP_CONNECTION_PRESENCE_TYPE_UNKNOWN;
    e.subscriptionState = "Requested";
    e.publishState = "No";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    /* Change the presence of Alice to busy */
    TpConnectionPresenceType presence = TP_CONNECTION_PRESENCE_TYPE_BUSY;
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

void TestTelepathyPlugin::testSelfContact()
{
    QContactLocalId selfId = mContactManager->selfContactId();
    QContact contact = mContactManager->contact(selfId);
    qDebug() << contact;

    TestExpectation e;
    e.flags = TestExpectation::VerifyFlags(TestExpectation::Presence | TestExpectation::Avatar);
    e.accountUri = QString("fake@account.org");
    e.presence = TP_CONNECTION_PRESENCE_TYPE_AVAILABLE;
    e.verify(contact);
}

void TestTelepathyPlugin::testAuthorization()
{
    /* Create a new contact "romeo" */
    TpHandle handle = ensureContact("romeo");

    /* Add Bob in the ContactList, the request will be ignored */
    test_contact_list_manager_request_subscription(mListManager, handle, "wait");

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
    test_contact_list_manager_request_subscription(mListManager, handle, "please");

    e.event = TestExpectation::Changed;
    e.subscriptionState = "Yes";
    e.publishState = "Requested";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    handle = ensureContact("juliette");

    /* Add Bob in the ContactList, the request will be ignored */
    test_contact_list_manager_request_subscription(mListManager, handle, "wait");

    e.event = TestExpectation::Added;
    e.accountUri = QString("juliette");
    e.subscriptionState = "Requested";
    e.publishState = "No";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    /* Ask again for subscription, but this time it will be rejected */
    test_contact_list_manager_request_subscription(mListManager, handle, "no");

    e.event = TestExpectation::Changed;
    e.subscriptionState = "No";
    e.publishState = "No";
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);
}

QList<QContactDetail> TestTelepathyPlugin::createContactInfo(GPtrArray **infoPtrArray)
{
    QList<QContactDetail> ret;
    *infoPtrArray = g_ptr_array_new_with_free_func((GDestroyNotify) g_value_array_free);

    gchar *randNumber = g_strdup_printf("%d", qrand());
    const gchar *fieldValues[] = { randNumber, NULL };

    g_ptr_array_add (*infoPtrArray, tp_value_array_build(3,
        G_TYPE_STRING, "tel",
        G_TYPE_STRV, NULL,
        G_TYPE_STRV, fieldValues,
        G_TYPE_INVALID));

    QContactPhoneNumber phoneNumber;
    phoneNumber.setContexts("Other");
    phoneNumber.setDetailUri(QString("tel:%1").arg(randNumber));
    phoneNumber.setNumber(QString("%1").arg(randNumber));
    phoneNumber.setSubTypes("Voice");
    ret << phoneNumber;

    g_free(randNumber);

    return ret;
}

void TestTelepathyPlugin::testContactInfo()
{
    /* Create a contact with no ContactInfo */
    TpHandle handle = ensureContact("skype");
    test_contact_list_manager_request_subscription(mListManager, handle, "wait");

    TestExpectation e;
    e.event = TestExpectation::Added;
    e.flags = TestExpectation::None;
    e.accountUri = QString("skype");
    mExpectations.append(e);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    GPtrArray *infoPtrArray;
    e.event = TestExpectation::Changed;
    e.flags = TestExpectation::Info;

    /* Set some ContactInfo on the contact */
    e.details = createContactInfo(&infoPtrArray);
    tp_tests_contacts_connection_change_contact_info(
        TP_TESTS_CONTACTS_CONNECTION(mConnService), handle, infoPtrArray);
    mExpectations.append(e);
    g_ptr_array_unref(infoPtrArray);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    /* Change the ContactInfo */
    e.details = createContactInfo(&infoPtrArray);
    tp_tests_contacts_connection_change_contact_info(
        TP_TESTS_CONTACTS_CONNECTION(mConnService), handle, infoPtrArray);
    mExpectations.append(e);
    g_ptr_array_unref(infoPtrArray);

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);
}

void TestTelepathyPlugin::testRemoveContacts()
{
    TestExpectation e;
    e.flags = TestExpectation::None;
    e.event = TestExpectation::Removed;
    e.nOnlineAccounts = 0;

    for (QHash<TpHandle, QString>::const_iterator i = mContacts.constBegin();
             i != mContacts.constEnd(); i++) {
        qDebug() << "removing" << i.key() << i.value();

        /* Removing from Stored list will remove from all lists */
        test_contact_list_manager_remove_contact(mListManager, i.key());

        e.accountUri = i.value();
        mExpectations.append(e);
    }

    /* Wait for the scenario to happen */
    QCOMPARE(mLoop->exec(), 0);

    mContacts.clear();
}

void TestTelepathyPlugin::testSetOffline()
{
    /* testRemoveContacts() was run before, so contact list is empty at this point. */
    TestExpectation e;
    e.flags = TestExpectation::None;
    e.accountUri = QString("kesh");

    TpHandle handle = ensureContact("kesh");
    test_contact_list_manager_request_subscription(mListManager, handle, "please");

    /* First contact is added, then auth req is accepted */
    e.event = TestExpectation::Added;
    mExpectations.append(e);
    e.event = TestExpectation::Changed;
    mExpectations.append(e);
    QCOMPARE(mLoop->exec(), 0);

    /* Now set the account offline, kesh should be updated to have Unknown presence */
    tp_base_connection_change_status(mConnService,
        TP_CONNECTION_STATUS_DISCONNECTED,
        TP_CONNECTION_STATUS_REASON_REQUESTED);

    e.flags = TestExpectation::Presence;
    e.event = TestExpectation::Changed;
    e.presence = TP_CONNECTION_PRESENCE_TYPE_UNKNOWN;
    mExpectations.append(e);
    QCOMPARE(mLoop->exec(), 0);
}

TpHandle TestTelepathyPlugin::ensureContact(const gchar *id)
{
    TpHandleRepoIface *serviceRepo =
        tp_base_connection_get_handles(mConnService, TP_HANDLE_TYPE_CONTACT);

    TpHandle handle = tp_handle_ensure(serviceRepo, id, NULL, NULL);
    mContacts[handle] = QString(id);

    return handle;
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
        QVERIFY2(!mExpectations.isEmpty(), "Was not expecting more events");

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

void TestTelepathyPlugin::contactsRemoved(const QList<QContactLocalId>& contactIds)
{
    qDebug() << "Got contactsRemoved";
    verify(TestExpectation::Removed, contactIds);
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
