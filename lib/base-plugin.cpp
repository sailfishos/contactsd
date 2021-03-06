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

#include "base-plugin.h"
#include "debug.h"

#include <QStandardPaths>

namespace Contactsd
{

const QString BasePlugin::metaDataKeyVersion = QString::fromLatin1("version");
const QString BasePlugin::metaDataKeyName    = QString::fromLatin1("name");
const QString BasePlugin::metaDataKeyComment = QString::fromLatin1("comment");


QDir
BasePlugin::cacheDir()
{
    const QString cacheRoot = QString::fromLatin1(".local/share/system/privileged/Contacts");
    QDir cacheDir = QDir::home().filePath(cacheRoot);

    if (not cacheDir.exists()) {
        if (not cacheDir.mkpath(QString::fromLatin1("."))) {
            qCWarning(lcContactsd) << "Could not create cache dir";
            return QDir();
        }
    }

    return cacheDir;
}

QString
BasePlugin::cacheFileName(const QString &fileName)
{
    return cacheDir().filePath(fileName);
}

} // Contactsd
