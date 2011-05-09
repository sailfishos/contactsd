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

#include "sparqlconnectionmanager.h"

#include <QtCore/QStringList>
#include <QtCore/QSettings>

#include <QtSparql/QSparqlConnection>

#include "debug.h"

using namespace Contactsd;

namespace com {
namespace nokia {
namespace contactsd {

////////////////////////////////////////////////////////////////////////////////////////////////////

const QStringList DefaultSparqlBackends = QStringList() << QLatin1String("QTRACKER_DIRECT")
                                                                     << QLatin1String("QTRACKER");
const QString SparqlBackendsKey = QLatin1String("sparqlBackends");

////////////////////////////////////////////////////////////////////////////////////////////////////

Q_GLOBAL_STATIC(SparqlConnectionManager, defaultSparqlManager);

////////////////////////////////////////////////////////////////////////////////////////////////////

SparqlConnectionManager::SparqlConnectionManager()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QSparqlConnection &
SparqlConnectionManager::defaultConnection()
{
    return defaultSparqlManager()->connection();
}

QSparqlConnection &
SparqlConnectionManager::connection()
{
    if (not m_connection.hasLocalData()) {
        static const QStringList permittedDriverNames =
                QSettings(QSettings::IniFormat, QSettings::UserScope, QLatin1String("Nokia"),
                          QLatin1String("Contacts")).value(SparqlBackendsKey, DefaultSparqlBackends).toStringList();
        bool validConnectionMissing = true;

        foreach(const QString &driverName, permittedDriverNames) {
            m_connection.setLocalData(new QSparqlConnection(driverName));

            if (m_connection.localData()->isValid()) {
                validConnectionMissing = false;
                break;
            }
        }

        if (validConnectionMissing) {
            QString permittedDrivers = permittedDriverNames.join(QLatin1String(", "));
            QString availableDrivers = QSparqlConnection::drivers().join(QLatin1String(", "));

            if (permittedDrivers.isEmpty()) {
                permittedDrivers = QLatin1String("none");
            }

            if (availableDrivers.isEmpty()) {
                availableDrivers = QLatin1String("none");
            }

            warning() << Q_FUNC_INFO << QString::fromLatin1("Could not find any usable QtSparql backend driver. "
                                        "Please install one of the configured drivers, or "
                                        "correct the setting. Configured backends: %1. "
                                        "Available backends: %2.").
                    arg(permittedDrivers, availableDrivers);

            m_connection.setLocalData(new QSparqlConnection(QLatin1String("invalid")));
        }
    }

    return *m_connection.localData();
}

}
}
}
