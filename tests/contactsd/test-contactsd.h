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

#ifndef TEST_CONTACTSD_H
#define TEST_CONTACTSD_H

#include <QObject>
#include <QtTest/QtTest>

#include <contactsd.h>

class TestContactsd : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();

    void testLoadAllPlugins();
    void testLoadPlugins();
    void testImportState();

    void cleanup();

private:
    Contactsd *mDaemon;
};

#endif // TEST_CONTACTSD_H
