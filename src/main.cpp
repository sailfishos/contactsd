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
#include <QLocale>
#include <QTranslator>
#include <QLoggingCategory>

#include <errno.h>
#include <signal.h>

#include "contactsd.h"
#include "debug.h"

using namespace Contactsd;

static bool useDebug = false;
static QLoggingCategory::CategoryFilter defaultCategoryFilter;

static void usage()
{
    QTextStream(stdout)
            << "Usage: contactsd [OPTION]...\n"
            << "\n"
            << "Options:\n"
            << "\n"
            << "  --plugins PLUGINS    Comma separated list of plugins to load\n"
            << "  --enable-debug       Enable debug logging\n"
            << "  --version            Output version information and exit\n"
            << "  --help               Display this help and exit\n"
            << "\n";
}

static void setupUnixSignalHandlers()
{
    struct sigaction sigterm, sigint;

    sigterm.sa_handler = ContactsDaemon::unixSignalHandler;
    sigemptyset(&sigterm.sa_mask);
    sigterm.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sigterm, 0) < 0) {
        qCWarning(lcContactsd) << "Could not setup signal handler for SIGTERM";
        return;
    }

    sigint.sa_handler = ContactsDaemon::unixSignalHandler;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sigint, 0) < 0) {
        qCWarning(lcContactsd) << "Could not setup signal handler for SIGINT";
        return;
    }
}

static void categoryFilter(QLoggingCategory *category)
{
    defaultCategoryFilter(category);

    if (qstrcmp(category->categoryName(), "contactsd") == 0 && useDebug) {
        category->setEnabled(QtDebugMsg, true);
        category->setEnabled(QtInfoMsg, true);
    }
}

Q_DECL_EXPORT int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QStringList plugins;
    useDebug = !qgetenv("CONTACTSD_DEBUG").isEmpty();

    const QStringList args = app.arguments();
    int i = 1; // ignore argv[0]

    while (i < args.count()) {
        QString arg = args.at(i);

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
        } else if (arg == "--enable-debug") {
            useDebug = true;
        } else {
            qWarning() << "Invalid argument" << arg;
            usage();
            return -1;
        }
        ++i;
    }

    defaultCategoryFilter = QLoggingCategory::installFilter(categoryFilter);
    setupUnixSignalHandlers();

    qCDebug(lcContactsd) << "contactsd version" << VERSION << "started";

    const QString translationPath(QString::fromLatin1(TRANSLATIONS_INSTALL_PATH));

    QScopedPointer<QTranslator> engineeringEnglish(new QTranslator);
    engineeringEnglish->load(QString::fromLatin1("contactsd_eng_en"), translationPath);
    app.installTranslator(engineeringEnglish.data());

    QScopedPointer<QTranslator> translator(new QTranslator);
    translator->load(QLocale(), QString::fromLatin1("contactsd"), QString::fromLatin1("-"), translationPath);
    app.installTranslator(translator.data());

    ContactsDaemon daemon;
    daemon.loadPlugins(plugins);

    const int rc = app.exec();

    return rc;
}
