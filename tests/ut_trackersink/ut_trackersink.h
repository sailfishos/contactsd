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

#ifndef UT_TRACKERSINK_H
#define UT_TRACKERSINK_H

#include <QObject>
#include <QtTest/QtTest>
#include <QString>
#include <qcontactrequests.h>
#include <tpcontact.h>

class TrackerSink;

/**
 * Telepathy support Tracker Sink unittests
 */
class ut_trackersink : public QObject
{
Q_OBJECT
public:
    ut_trackersink();

private Q_SLOTS
    void initTestCase();
    void cleanupTestCase();
    void testImContactForTpContactId();
    void testImContactForPeople();
    void testPersonContactForTpContactId();
    void testSinkToStorage();
    void testOnSimplePresenceChanged();
    void sinkContainsImAccount();

protected Q_SLOTS:
    void contactsAdded(const QList<QContactLocalId>& contactIds);
    void contactsChanged(const QList<QContactLocalId>& contactIds);

private:
    TrackerSink* const sink;
    unsigned int contactInTrackerUID;
    TpContactPtr telepathier;
    QList<QContactLocalId> added;
    QList<QContactLocalId> changed;
};

class Slots: public QObject
{
    Q_OBJECT
public:
    QList<QContact> contacts;
public Q_SLOTS:
    void progress(QContactFetchRequest* self, bool appendOnly);
};

#endif
