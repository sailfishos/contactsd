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

QTM_USE_NAMESPACE

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
    TpTestsSimpleAccountManager *mAccountManagerService;
    TpTestsSimpleAccount *mAccountService;
    TpBaseConnection *mConnService;
    ConnectionPtr mConn;

    QContactManager *manager;
    QList<QContactLocalId> added;
    QList<QContactLocalId> changed;
};

#endif
