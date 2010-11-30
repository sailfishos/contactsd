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

#include <TelepathyQt4/Contact>

#include "tests/lib/glib/simple-account-manager.h"
#include "tests/lib/glib/simple-account.h"
#include "tests/lib/glib/contacts-conn.h"
#include "tests/lib/glib/util.h"

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
        None          = 0,
        Alias         = (1 << 0),
        Presence      = (1 << 1),
        Avatar        = (1 << 2),
        Authorization = (1 << 3),
        Info          = (1 << 4),
        All           = (1 << 5) - 1
    };

    TestExpectation();
    void verify(QContact &contact) const;

    VerifyFlags flags;
    Event event;

    int nOnlineAccounts;
    QString accountUri;

    QString alias;
    TpTestsContactsConnectionPresenceStatusIndex presence;
    QByteArray avatarData;
    QString subscriptionState;
    QString publishState;
    QList<QContactDetail> details;
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

private Q_SLOTS:
    void initTestCase();
    void init();

    void testBasicUpdates();
    void testSelfContact();
    void testAuthorization();
    void testContactInfo();
    void testRemoveContacts();
    void testSetOffline();

    void cleanup();
    void cleanupTestCase();

private:
    QContactManager *mContactManager;
    TpBaseConnection *mConnService;
    TestContactListManager *mListManager;

    QHash<TpHandle, QString> mContacts;
    TpHandle ensureContact(const gchar *id);

    QList<TestExpectation> mExpectations;
    void verify(TestExpectation::Event, const QList<QContactLocalId>&);

    QList<QContactDetail> createContactInfo(GPtrArray **infoPtrArray);
};

#endif
