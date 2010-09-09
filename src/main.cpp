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

#include <QDebug>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QTimer>

#include "contactsd.h"
#include "logger.h"

static void usage()
{
    qDebug() << "Usage: contactsd [OPTION]...\n";
    qDebug() << "  --plugins PLUGINS    comma separated list of plugins to load";
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QStringList plugins;
    const QStringList args = app.arguments();
    QString arg;
    int i = 1; // ignore argv[0]
    while (i < args.count()) {
        arg = args.at(i);
        if (arg == "--plugins") {
            if (++i == args.count()) {
                usage();
                return -1;
            }

            QString value = args.at(i);
            value.replace(" ", ",");
            plugins << value.split(",", QString::SkipEmptyParts);
        } else {
            qWarning() << "Invalid argument" << arg;
            usage();
            return -1;
        }
        ++i;
    }

#ifndef QT_NO_DEBUG_OUTPUT
    Logger *logger = Logger::installLogger(CONTACTSD_LOG_DIR "/contactsd.log", 50, 3);
    logger->setParent(&app);
#endif

    qDebug() << "=== Contacts Daemon Start ===";
    qDebug() << "=== Based on" << VERSION_INFO;
    qDebug() << "=== Revision" << REVISION_INFO;
    qDebug() << "=== Built on" << BUILDDATE_INFO << BUILDTIME_INFO;
    qDebug() << "=============================";

    Contactsd *daemon = new Contactsd(&app);
    daemon->loadPlugins(plugins);

    return app.exec();
}
