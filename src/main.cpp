/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#include <QDebug>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QTimer>

#include "contactsdaemon.h"

//#include "socialprovidermanager.h"
//TODO: This seems to use the installed header instead of the local one:
//#include "socialprovider/socialprovidermanager.h"

#include "logger.h"

int main (int argc, char **argv)
{
#ifndef QT_NO_DEBUG_OUTPUT
    Logger *logger = Logger::installLogger(QString("/var/log/contactsd.log"), 50, 3);
#endif

    qDebug() << Q_FUNC_INFO << "=== Contacts Daemon Start ===";
    qDebug() << Q_FUNC_INFO << "=== Based on" << VERSION_INFO;
    qDebug() << Q_FUNC_INFO << "=== Revision" << REVISION_INFO;
    qDebug() << Q_FUNC_INFO << "=== Built on" << BUILDDATE_INFO << BUILDTIME_INFO;
    qDebug() << Q_FUNC_INFO << "==============================";

    QCoreApplication app(argc, argv);

#ifndef QT_NO_DEBUG_OUTPUT
    if (logger) {
        // set parent to application to get destructor called
        logger->setParent(&app);
    }
#endif

    //Allow developers to request a clean stop,
    //so valgrind can check for leaks.
    const QStringList arguments = app.arguments();
    if (arguments.contains("--debugleakcheck")) {
        QTimer::singleShot( 120000, &app, SLOT(quit()) );
    }

    ContactsDaemon* daemon = new ContactsDaemon(&app);
    int index = 0;
    foreach (const QString &arg, arguments) {
        if (arg == "--plugins") {
            index = app.arguments().indexOf(arg);
            break;
        }
    }

    QStringList plugins;
    if (index != 0 ) {
        for (int i = index + 1 ; i< app.arguments().count(); i++) {
            qDebug() << Q_FUNC_INFO << app.arguments().at(i);
            plugins << app.arguments().at(i);
        }
    }
    daemon->loadAllPlugins(plugins);

    Q_UNUSED(daemon);

    const int result = app.exec();

    //To help with leak detection:
    //SocialProviderManager::cleanupInstance();

    return result;
}
