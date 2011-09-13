/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include <QCoreApplication>
#include <QDBusConnection>
#include <QTimer>

#include <signal.h>

#include "contactsd.h"
#include "debug.h"

using namespace Contactsd;

static QtMsgHandler defaultMsgHandler = 0;

static void nullMsgHandler(QtMsgType type, const char *msg)
{
    if (QtDebugMsg == type) {
        return; // no debug messages please
    }

    // Actually qInstallMsgHandler() returned null in main() when
    // I checked, so defaultMsgHandler should be null - but let's be careful.
    if (defaultMsgHandler) {
        defaultMsgHandler(type, msg);
    } else {
        fprintf(stderr, "%s\n", msg);
    }
}

static void usage()
{
    qDebug() << "Usage: contactsd [OPTION]...\n";
    qDebug() << "  --plugins PLUGINS    Comma separated list of plugins to load\n";
    qDebug() << "  --log-console        Enable Console Logging \n";
    qDebug() << "  --version            Output version information and exit";
    qDebug() << "  --help               Display this help and exit";
}

static void setupUnixSignalHandlers()
{
    struct sigaction sigterm, sigint;

    sigterm.sa_handler = ContactsDaemon::unixSignalHandler;
    sigemptyset(&sigterm.sa_mask);
    sigterm.sa_flags |= SA_RESTART;

    if (sigaction(SIGTERM, &sigterm, 0) > 0) {
        qWarning() << "Could not setup signal handler for SIGTERM";
        return;
    }

    sigint.sa_handler = ContactsDaemon::unixSignalHandler;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &sigint, 0) > 0) {
        qWarning() << "Could not setup signal handler for SIGINT";
        return;
    }
}

int main(int argc, char **argv)
{
    setupUnixSignalHandlers();

    QCoreApplication app(argc, argv);

    QStringList plugins;
    const QStringList args = app.arguments();
    QString arg;
    int i = 1; // ignore argv[0]

    bool logConsole = !qgetenv("CONTACTSD_DEBUG").isEmpty();

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
        } else if (arg == "--version") {
            qDebug() << "contactsd version" << VERSION;
            return 0;
        } else if (arg == "--help") {
            usage();
            return 0;
        } else if (arg == "--log-console") {
            logConsole = true;
        } else {
            qWarning() << "Invalid argument" << arg;
            usage();
            return -1;
        }
        ++i;
    }

    if (not logConsole) {
        defaultMsgHandler = qInstallMsgHandler(nullMsgHandler);
    }

    enableDebug(logConsole);
    debug() << "contactsd version" << VERSION << "started";

    ContactsDaemon *daemon = new ContactsDaemon(&app);
    daemon->loadPlugins(plugins);

    return app.exec();
}
