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
#include <QEventLoop>
#include <QContactManager>

#include <telepathy-glib/telepathy-glib.h>

#include "lib/contact-list-manager.h"

QTM_USE_NAMESPACE

 class TestBenchmark : public QObject
{
Q_OBJECT
public:
    TestBenchmark(QObject *parent = 0);
    ~TestBenchmark();

    void exec(int waitAdded, int waitChanged, int waitRemoved);
    void addContacts(int number);
    void removeAllContacts();
    void setStatus(TpConnectionStatus status);

protected Q_SLOTS:
    void contactsAdded(const QList<QContactLocalId>& contactIds);
    void contactsChanged(const QList<QContactLocalId>& contactIds);
    void contactsRemoved(const QList<QContactLocalId>& contactIds);

private:
    void verifyFinished();
    void randomizeContact(TpHandle handle);

    QContactManager *mContactManager;
    TpBaseConnection *mConnService;
    TestContactListManager *mListManager;

    int mWaitAdded;
    int mWaitChanged;
    int mWaitRemoved;
    GArray *mHandles;
    QEventLoop *mLoop;
};

#endif
