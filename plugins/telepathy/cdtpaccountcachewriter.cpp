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

#include "cdtpaccountcachewriter.h"

#include <debug.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cdtpaccountcache.h"

using namespace Contactsd;

///////////////////////////////////////////////////////////////////////////////

CDTpAccountCacheWriter::CDTpAccountCacheWriter(const CDTpAccount *account,
                                               QObject *parent)
    : QObject(parent)
    , mAccount(account)
{
}

void CDTpAccountCacheWriter::run()
{
    const QString accountPath = mAccount->account()->objectPath();
    const QString rosterFileName = CDTpAccountCache::cacheFilePath(mAccount);
    const QHash<QString, CDTpContact::Info> cache = mAccount->rosterCache();

    if (cache.isEmpty()) {
        QFile(rosterFileName).remove();
        return;
    }

    QTemporaryFile tempFile(rosterFileName);
    tempFile.setAutoRemove(false);

    if (not tempFile.open()) {
        warning() << "Could not open file" << tempFile.fileName()
                  << "for writing:" << tempFile.errorString();
        tempFile.setAutoRemove(true);
        return;
    }

    QByteArray data;
    QBuffer buffer(&data);

    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream << CDTpAccountCache::Version;
    stream << cache;

    buffer.close();

    if (tempFile.write(data) != data.size()) {
        warning() << "Could not write roster cache for account" << accountPath << ":" << tempFile.errorString();
        tempFile.setAutoRemove(true);
        return;
    }

    if (not tempFile.flush()
     || (::fsync(tempFile.handle()) != 0)
     || (tempFile.close(), false)) {
        warning() << "Could not finalize roster cache for account" << accountPath << ":" << tempFile.errorString();
        tempFile.setAutoRemove(true);
        return;
    }

    if (::rename(tempFile.fileName().toLocal8Bit(), rosterFileName.toLocal8Bit()) != 0) {
        warning() << "Could not write roster cache for account" << accountPath << ":" << strerror(errno);
        tempFile.setAutoRemove(true);
        return;
    }

    debug() << "Wrote" << mAccount->rosterCache().size() << "contacts to cache for account" << accountPath;
}
