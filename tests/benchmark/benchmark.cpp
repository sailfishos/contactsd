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

#include <QDebug>
#include <QCoreApplication>

#include "tests/lib/glib/simple-account-manager.h"
#include "tests/lib/glib/simple-account.h"
#include "tests/lib/glib/contacts-conn.h"
#include "tests/lib/glib/util.h"

#include "benchmark.h"

#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "fakecm/fakeproto/fakeaccount"
#define BUS_NAME "org.maemo.Contactsd.UnitTest"

TestBenchmark::TestBenchmark(QObject *parent) :
        QObject(parent), mHandles(g_array_new(FALSE, FALSE, sizeof (TpHandle))),
        mLoop(new QEventLoop(this))
{
    g_type_init();
    g_set_prgname("test-benchmark");
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
    tp_base_connection_register(mConnService, "fakecm", NULL, NULL, NULL);
    TpHandleRepoIface *serviceRepo = tp_base_connection_get_handles(
        mConnService, TP_HANDLE_TYPE_CONTACT);
    mConnService->self_handle = tp_handle_ensure(serviceRepo,
        "fake@account.org", NULL, NULL);
    tp_base_connection_change_status(mConnService,
        TP_CONNECTION_STATUS_CONNECTING,
        TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED);
    mListManager = tp_tests_contacts_connection_get_contact_list_manager(
        TP_TESTS_CONTACTS_CONNECTION(mConnService));

    /* Define the self contact */
    TpTestsContactsConnectionPresenceStatusIndex presence =
            TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE;
    const gchar *message = "Running benchmark";
    tp_tests_contacts_connection_change_presences(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &mConnService->self_handle, &presence, &message);
    const gchar *aliases = "badger";
    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &mConnService->self_handle, &aliases);

    /* Request the UnitTest bus name, so the AM knows we are ready to go */
    TpDBusDaemon *dbus = tp_dbus_daemon_dup(NULL);
    tp_dbus_daemon_request_name(dbus, BUS_NAME, FALSE, NULL);
    g_object_unref (dbus);
}

TestBenchmark::~TestBenchmark()
{
    tp_base_connection_change_status(mConnService,
        TP_CONNECTION_STATUS_DISCONNECTED,
        TP_CONNECTION_STATUS_REASON_REQUESTED);

    g_object_unref(mConnService);
    delete mContactManager;
    delete mLoop;
    g_array_unref(mHandles);
}


void TestBenchmark::contactsAdded(const QList<QContactLocalId>& contactIds)
{
    mWaitAdded -= contactIds.count();
    verifyFinished();
}

void TestBenchmark::contactsChanged(const QList<QContactLocalId>& contactIds)
{
    mWaitChanged -= contactIds.count();
    verifyFinished();
}

void TestBenchmark::contactsRemoved(const QList<QContactLocalId>& contactIds)
{
    mWaitRemoved -= contactIds.count();
    verifyFinished();
}

void TestBenchmark::verifyFinished()
{
    if (mWaitAdded <= 0 && mWaitChanged <= 0 && mWaitRemoved <= 0) {
        mLoop->quit();
    }
}

void TestBenchmark::exec(int waitAdded, int waitChanged, int waitRemoved)
{
    mWaitAdded = waitAdded;
    mWaitChanged = waitChanged;
    mWaitRemoved = waitRemoved;

    QTime t;
    t.start();
    mLoop->exec();
    qDebug() << t.elapsed() << "ms elapsed";
}

#define LETTERS "abcdefghijklmnopqrstuvwxyz"
#define NUMBERS "0123456789"
#define ALNUM LETTERS NUMBERS
#define TEXT ALNUM " "

static gchar *randomString(int len, const gchar *chars = LETTERS)
{
    gchar *result = g_new0(gchar, len + 1);
    for (int i = 0; i < len; i++) {
        result[i] = chars[qrand() % strlen(chars)];
    }

    return result;
}

void TestBenchmark::randomizeContact(TpHandle handle)
{
    Q_UNUSED(handle);
#if 0
    TpTestsContactsConnectionPresenceStatusIndex presence =
        TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY;
    const gchar *message = "Making coffee";
    tp_tests_contacts_connection_change_presences(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &presence, &message);

    tp_tests_contacts_connection_change_aliases(
        TP_TESTS_CONTACTS_CONNECTION (mConnService),
        1, &handle, &alias);
#endif
}

void TestBenchmark::addContacts(int number)
{
    TpHandleRepoIface *serviceRepo =
        tp_base_connection_get_handles(mConnService, TP_HANDLE_TYPE_CONTACT);

    for (int i = 0; i < number; i++) {
        gchar *id = randomString(20);
        TpHandle handle = tp_handle_ensure(serviceRepo, id, NULL, NULL);
        g_free(id);

        randomizeContact(handle);

        g_array_append_val (mHandles, handle);
    }

    qDebug() << "Adding" << number << "contacts...";
    test_contact_list_manager_request_subscription(mListManager,
        mHandles->len, (TpHandle *) mHandles->data, "wait");
}

void TestBenchmark::removeAllContacts()
{
    qDebug() << "Removing" << mHandles->len << "contacts...";
    test_contact_list_manager_remove(mListManager,
        mHandles->len, (TpHandle *) mHandles->data);
    g_array_remove_range(mHandles, 0, mHandles->len);
}

void TestBenchmark::setStatus(TpConnectionStatus status)
{
    const gchar *status_str;
    switch (status) {
    case TP_CONNECTION_STATUS_CONNECTED:
        status_str = "connected";
        break;
    case TP_CONNECTION_STATUS_CONNECTING:
        status_str = "connecting";
        break;
    case TP_CONNECTION_STATUS_DISCONNECTED:
        status_str = "disconnected";
        break;
    }

    qDebug() << "Changing status to" << status_str << "with" << mHandles->len << "contacts...";
    tp_base_connection_change_status(mConnService, status,
        TP_CONNECTION_STATUS_REASON_REQUESTED);
}

#define N_CONTACTS 5000

int main (int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    TestBenchmark benchmark;

    /* Add contacts before being connected. When account becomes connected, it
     * will also remove all contacts from tracker that are not in the initial
     * set of contacts */
    benchmark.addContacts(N_CONTACTS);
    benchmark.setStatus(TP_CONNECTION_STATUS_CONNECTED);
    benchmark.exec(N_CONTACTS, 0, 0);

    /* Remove all contacts */
    benchmark.removeAllContacts();
    benchmark.exec(0, 0, N_CONTACTS);

    /* Add a new set of contacts */
    benchmark.addContacts(N_CONTACTS);
    benchmark.exec(N_CONTACTS, 0, 0);

    /* On disconnect, all contacts are set offline */
    benchmark.setStatus(TP_CONNECTION_STATUS_DISCONNECTED);
    benchmark.exec(0, N_CONTACTS, 0);
}

