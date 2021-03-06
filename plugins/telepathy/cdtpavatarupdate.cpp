/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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
 *********************************************************************************/


#include "cdtpavatarupdate.h"
#include "cdtpplugin.h"
#include "debug.h"

using namespace Contactsd;

const QString CDTpAvatarUpdate::Large = QLatin1String("large");
const QString CDTpAvatarUpdate::Square = QLatin1String("square");

void CDTpAvatarUpdate::updateContact(CDTpContact *contactWrapper, QNetworkReply *networkReply,
                                     const QString &filename, const QString &avatarType)
{
    (void) new CDTpAvatarUpdate(networkReply, contactWrapper, filename, avatarType);
}

CDTpAvatarUpdate::CDTpAvatarUpdate(QNetworkReply *networkReply,
                                   CDTpContact *contactWrapper,
                                   const QString &filename,
                                   const QString &avatarType)
    : QObject()
    , mNetworkReply(0)
    , mContactWrapper(contactWrapper)
    , mFilename(filename)
    , mAvatarType(avatarType)
{
    setNetworkReply(networkReply);
}

void CDTpAvatarUpdate::setNetworkReply(QNetworkReply *networkReply)
{
    if (mNetworkReply) {
        mNetworkReply->disconnect(this);
        mNetworkReply->deleteLater();
    }

    mNetworkReply = networkReply;

    if (mNetworkReply) {
        if (mNetworkReply->isRunning()) {
            connect(mNetworkReply, SIGNAL(finished()), this, SLOT(onRequestDone()));
            connect(mNetworkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onRequestDone()));
        } else {
            onRequestDone();
        }
    } else {
        // This operation is complete
        this->deleteLater();
    }
}

void CDTpAvatarUpdate::onRequestDone()
{
    QNetworkReply *newReply = 0;

    if (mNetworkReply && mNetworkReply->error() == QNetworkReply::NoError) {
        newReply = updateContactAvatar();
    }

    setNetworkReply(newReply);
}

QString CDTpAvatarUpdate::writeAvatarFile(QFile &avatarFile, const QDir &cacheDir)
{
    if (not cacheDir.exists() && not QDir::root().mkpath(cacheDir.absolutePath())) {
        qCWarning(lcContactsd) << "Could not create large avatar cache dir:" << cacheDir.path();
        return QString();
    }

    QTemporaryFile tempFile(cacheDir.absoluteFilePath(QLatin1String("pinkpony")));
    const QByteArray data = mNetworkReply->readAll();

    if (tempFile.open() && data.count() == tempFile.write(data)) {
        tempFile.close();

        if (avatarFile.exists()) {
            avatarFile.remove();
        }

        if (tempFile.rename(avatarFile.fileName())) {
            tempFile.setAutoRemove(false);
            return avatarFile.fileName();
        }
    }

    return QString();
}

static bool acceptFileSize(qint64 actualFileSize, qint64 expectedFileSize)
{
    if (expectedFileSize > 0) {
        return (actualFileSize == expectedFileSize);
    }

    return actualFileSize > 0;
}

QNetworkReply *CDTpAvatarUpdate::updateContactAvatar()
{
    // Build filename from the image URL's SHA1 hash.
    const QUrl redirectionTarget = mNetworkReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    const QString avatarUrl = (not redirectionTarget.isEmpty() ? mNetworkReply->url().resolved(redirectionTarget)
                                                               : mNetworkReply->url()).toString();

    QString filename(mFilename);
    if (filename.isEmpty()) {
        QByteArray avatarHash = QCryptographicHash::hash(avatarUrl.toUtf8(), QCryptographicHash::Sha1);
        filename = QString::fromLatin1(avatarHash.toHex());
    }

    const QDir cacheDir(CDTpPlugin::cacheFileName(QLatin1String("avatars")));
    QFile avatarFile(cacheDir.absoluteFilePath(filename));

    // Check for existing avatar file and its size to see if we need to fetch from network.
    const qint64 contentLength = mNetworkReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

    QString avatarPath;
    if (avatarFile.exists() && acceptFileSize(avatarFile.size(), contentLength)) {
        // Seems we can reuse the existing avatar file.
        avatarPath = avatarFile.fileName();
    } else {
        // Follow redirections as done by Facebook's graph API.
        if (not redirectionTarget.isEmpty()) {
            return mNetworkReply->manager()->get(QNetworkRequest(redirectionTarget));
        }

        // Facebook delivers a distinct gif image if no avatar is set. Ignore that bugger.
        const QString contentType = mNetworkReply->header(QNetworkRequest::ContentTypeHeader).toString();

        static const QLatin1String contentTypeImageGif = QLatin1String("image/gif");
        static const QLatin1String contentTypeImage = QLatin1String("image/");

        if (contentType.startsWith(contentTypeImage) && contentType != contentTypeImageGif) {
            avatarPath = writeAvatarFile(avatarFile, cacheDir);
        }
    }

    // Update the contact if a new avatar is available.
    if (not avatarPath.isEmpty() && not mContactWrapper.isNull()) {
        if (mAvatarType == Square) {
            mContactWrapper->setSquareAvatarPath(avatarPath);
        } else if (mAvatarType == Large) {
            mContactWrapper->setLargeAvatarPath(avatarPath);
        }
    }

    // No further work to do
    return 0;
}
