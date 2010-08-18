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

#include <QContactManager>

#include <tpcontactstub.h>

QTM_USE_NAMESPACE

class TrackerSink;

/**
 * Telepathy plugin's TrackerSink unit test
 */
class ut_trackersink : public QObject
{
Q_OBJECT
public:
    ut_trackersink();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testSinkToStorage();
    void testOnSimplePresenceChanged();

protected Q_SLOTS:
    void contactsAdded(const QList<QContactLocalId>& contactIds);
    void contactsChanged(const QList<QContactLocalId>& contactIds);

private:
    TrackerSink* const sink;
    QContactManager *manager;
    TpContactStub *telepathier;
    unsigned int contactInTrackerUID;
    QList<QContactLocalId> added;
    QList<QContactLocalId> changed;
};

class Slots: public QObject
{
    Q_OBJECT
public:
    QList<QContact> contacts;
};

#endif
