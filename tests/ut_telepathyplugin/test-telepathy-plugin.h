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

#ifndef TEST_TELEPATHY_PLUGIN_H
#define TEST_TELEPATHY_PLUGIN_H

#include <QObject>
#include <QtTest/QtTest>
#include <QString>
#include <QContactManager>

#include <telepathy-glib/telepathy-glib.h>
#include <TelepathyQt4/Contact>

#include "libtelepathy/contacts-conn.h"
#include "libtelepathy/contact-list-manager.h"
#include "libtelepathy/simple-account-manager.h"
#include "libtelepathy/simple-account.h"

#include "test.h"

QTM_USE_NAMESPACE

class TestExpectation
{
public:
    enum Event {
        Added,
        Changed,
        Removed
    };

    enum VerifyFlags {
        None           = 0,
        Alias          = (1 << 0),
        Presence       = (1 << 1),
        Avatar         = (1 << 2),
        Authorization  = (1 << 3),
        Info           = (1 << 4),
        OnlineAccounts = (1 << 5),
        All            = (1 << 6) - 1
    };

    TestExpectation();
    void verify(QContact &contact) const;

    int flags;
    Event event;

    int nOnlineAccounts;
    QString accountUri;

    QString alias;
    TpTestsContactsConnectionPresenceStatusIndex presence;
    QByteArray avatarData;
    QString subscriptionState;
    QString publishState;
    QList<QContactDetail> details;

    bool fetching;
    QContactManager::Error fetchError;
};

/**
 * Telepathy plugin's unit test
 */
class TestTelepathyPlugin : public Test
{
Q_OBJECT
public:
    TestTelepathyPlugin(QObject *parent = 0);

protected Q_SLOTS:
    void contactsAdded(const QList<QContactLocalId>& contactIds);
    void contactsChanged(const QList<QContactLocalId>& contactIds);
    void contactsRemoved(const QList<QContactLocalId>& contactIds);
    void onContactsFetched();

private Q_SLOTS:
    void initTestCase();
    void init();

    /* Generic tests */
    void testBasicUpdates();
    void testSelfContact();
    void testAuthorization();
    void testContactInfo();
    void testRemoveContacts();
    void testSetOffline();

    /* Specific tests */
    void testBug220851();

    void cleanup();
    void cleanupTestCase();

private:
    enum TestState {
        TestStateNone,
        TestStateInit,
        TestStateReady,
        TestStateDisconnecting,
        TestStateCleanup
    } mState;

    QContactManager *mContactManager;
    TpTestsSimpleAccountManager *mAccountManager;
    TpTestsSimpleAccount *mAccount;

    TpBaseConnection *mConnService;
    TpConnection *mConnection;
    TestContactListManager *mListManager;

    TpHandle ensureContact(const gchar *id);
    int mContactCount;

    TestExpectation mExpectation;
    void verify(TestExpectation::Event, const QList<QContactLocalId>&);
    void fetchAndVerifyContacts(const QList<QContactLocalId> &contactIds);

    QList<QContactDetail> createContactInfoTel(GPtrArray **infoPtrArray,
        const gchar *number);
};

#endif
