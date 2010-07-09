/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

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

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testImContactForTpContactId();
    void testImContactForPeople();
    void testPersonContactForTpContactId();
    void testSinkToStorage();
    void testOnSimplePresenceChanged();
    void sinkContainsImAccount();

protected slots:
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
public slots:
    void progress(QContactFetchRequest* self, bool appendOnly);
};

#endif
