/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#ifndef TEST_BIRTHDAYPLUGIN_H
#define TEST_BIRTHDAYPLUGIN_H

#include <QObject>
#include <QtTest/QtTest>

#include <event.h>

#include <QContact>
#include <QContactManager>

QTM_USE_NAMESPACE

class TestBirthdayPlugin : public QObject
{
    Q_OBJECT

public:
    explicit TestBirthdayPlugin(QObject *parent = 0);

private Q_SLOTS:
    void init();
    void initTestCase();

    void testAddAndRemoveBirthday();
    void testChangeBirthday();
    void testChangeName();
    void testLocaleChange();

    void cleanupTestCase();
    void cleanup();

private:
    int countCalendarEvents(const KCalCore::Event::List &eventList,
                            const QContact &contact) const;
    bool saveContact(QContact &contact);

private:
    QContactManager *mManager;
    QSet<QContactLocalId> mContactLocalIDs;
};

#endif // TEST_BIRTHDAYPLUGIN_H
