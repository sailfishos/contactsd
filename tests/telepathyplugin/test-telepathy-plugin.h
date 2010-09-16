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
    };

    void verify(QContact &contact) const;

    Event event;
    QString accountUri;
    QString alias;
    TpTestsContactsConnectionPresenceStatusIndex presence;
    QByteArray avatarData;
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

private Q_SLOTS:
    void initTestCase();
    void init();

    void testTrackerImport();

    void cleanup();
    void cleanupTestCase();

private:
    QContactManager *mContactManager;
    TpTestsSimpleAccountManager *mAccountManagerService;
    TpTestsSimpleAccount *mAccountService;
    TpBaseConnection *mConnService;

    QList<TestExpectation> mExpectations;
    void verify(TestExpectation::Event, const QList<QContactLocalId>&);
};

#endif
