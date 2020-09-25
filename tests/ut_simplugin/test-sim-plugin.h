/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2013 Jolla Ltd.
 **
 ** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **/

#ifndef TEST_SIM_PLUGIN_H
#define TEST_SIM_PLUGIN_H

#include <QObject>
#include <QtTest/QtTest>

// "unprotect" m_phonebook.onModemInterfacesChanged
#define private public
#define protected public
#include "cdsimcontroller.h"
#undef private
#undef protected

class TestSimPlugin : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.nemomobile.contactsd.test-sim")

public:
    explicit TestSimPlugin(QObject *parent = 0);

private Q_SLOTS:
    void init();
    void initTestCase();

    void testAdd();
    void testAppend();
    void testRemove();
    void testAddAndRemove();
    void testChangedNumber();
    void testMultipleNumbers();
    void testMultipleIdenticalNumbers();
    void testTrimWhitespace();
    void testCoalescing();
    void testEmpty();
    void testClear();

    void cleanupTestCase();
    void cleanup();

private:
    QList<QContact> getAllSimContacts(const QContactManager &m);
    QContact createTestContact();

    CDSimController *m_controller;
    QContactCollection m_collection;
};

#endif // TEST_SIM_PLUGIN_H
