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
#include <QTest>
#include <QString>
#include <QContactManager>

#include <telepathy-glib/telepathy-glib.h>
#include <TelepathyQt4/Contact>

#include "libtelepathy/contacts-conn.h"
#include "libtelepathy/contact-list-manager.h"
#include "libtelepathy/simple-account-manager.h"
#include "libtelepathy/simple-account.h"

#include "test.h"
#include "test-expectation.h"

QTM_USE_NAMESPACE

class TestFetchContacts;

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

private Q_SLOTS:
    void initTestCase();
    void init();

    /* Generic tests */
    void testBasicUpdates();
    void testSelfContact();
    void testAuthorization();
    void testContactInfo();
    void testRemoveContacts();
    //void testRemoveBuddyDBusAPI();
    void testInviteBuddyDBusAPI();
    void testSetOffline();
    void testAvatar();

    /* Specific tests */
    void testBug220851();
    void testIRIEncode();

    void cleanup();
    void cleanupTestCase();

private:
    TpHandle ensureContact(const gchar *id);
    GPtrArray *createContactInfoTel(const gchar *number);
    void verify(Event event, const QList<QContactLocalId> &contactIds);
    void runExpectation(TestExpectation *expectation);

private:
    friend class TestFetchContacts;
    QContactManager *mContactManager;
    TpTestsSimpleAccountManager *mAccountManager;
    TpTestsSimpleAccount *mAccount;

    TpBaseConnection *mConnService;
    TpConnection *mConnection;
    TestContactListManager *mListManager;

    int mContactCount;

    TestExpectation *mExpectation;
};

class TestFetchContacts : public QObject
{
    Q_OBJECT

public:
    TestFetchContacts(TestTelepathyPlugin *test, Event event, const QList<QContactLocalId> &contactIds);

private Q_SLOTS:
    void onContactsFetched();

private:
    TestTelepathyPlugin *mTest;
    Event mEvent;
    QList<QContactLocalId> mContactIds;
};

#endif
