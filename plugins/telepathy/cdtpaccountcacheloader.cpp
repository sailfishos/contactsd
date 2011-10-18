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

#include "cdtpaccountcacheloader.h"

#include "cdtpaccountcache.h"

#include <debug.h>

using namespace Contactsd;

CDTpAccountCacheLoader::CDTpAccountCacheLoader(CDTpAccount *account, QObject *parent)
    : QObject(parent)
    , mAccount(account)
{
}

void CDTpAccountCacheLoader::run()
{
    const QString accountPath = mAccount->account()->objectPath();
    QFile cacheFile(CDTpAccountCache::cacheFilePath(mAccount));

    if (not cacheFile.exists()) {
        debug() << Q_FUNC_INFO << "Account" << accountPath << "has no cache file";
        return;
    }

    if (not cacheFile.open(QIODevice::ReadOnly)) {
        warning() << Q_FUNC_INFO << "Can't open" << cacheFile.fileName() << "for reading:"
                  << cacheFile.error();
        return;
    }

    QByteArray cacheData = cacheFile.readAll();
    cacheFile.close();

    QDataStream stream(cacheData);

    if (stream.atEnd()) {
        debug() << Q_FUNC_INFO << "Empty cache file" << cacheFile.fileName();
        cacheFile.remove();
        return;
    }

    int cacheVersion;
    stream >> cacheVersion;

    if (cacheVersion != CDTpAccountCache::Version) {
        warning() << "Wrong cache version for file" << cacheFile.fileName();
        cacheFile.remove();
    }

    QHash<QString, CDTpContact::Info> cache;
    stream >> cache;

    mAccount->setRosterCache(cache);

    debug() << "Loaded" << cache.size() << "contacts from cache for account" << accountPath;
}

