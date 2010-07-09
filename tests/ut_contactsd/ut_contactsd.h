/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#ifndef UT_CONTACTSD_H
#define UT_CONTACTSD_H

#include <QObject>
#include <QtTest/QtTest>
#include <contactsdaemon.h>


/**
 * Contactsd tests
 */

class ut_contactsd : public QObject
{
Q_OBJECT

public:
    bool getPluginsLoaded() { return plugsLoaded; }

private slots:
    void initTestCase();

    void testLoadAllPlugins();
    void testLoadPlugins();

    void cleanupTestCase();

public slots:
    void pluginsLoaded();
private:
    ContactsDaemon* daemon;
    bool plugsLoaded;
};

#endif // UT_CONTACTSD_H
