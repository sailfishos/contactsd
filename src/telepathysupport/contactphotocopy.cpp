#include <QDir>
#include <QDebug>
#include <QImage>
#include "contactphotocopy.h"

QStringList ContactPhotoCopy::avatarSearchPaths;

bool ContactPhotoCopy::copyToAvatarDir(const QString &fullFilePath, QString &newFileName)
{
    return ContactPhotoCopy::copyToAvatarDir(fullFilePath, QSize(), ContactPhotoCopy::PhotoEditLeaveUntouched, newFileName);
}

bool ContactPhotoCopy::copyToAvatarDir(const QString& fullFilePath, const QSize& scaledSize, QString& newFileName)
{
    return ContactPhotoCopy::copyToAvatarDir(fullFilePath, scaledSize, ContactPhotoCopy::PhotoEditCropAndScale, newFileName);
}

bool ContactPhotoCopy::copyToAvatarDir(const QString& fullFilePath, const QSize& scaledSize,
                                       PhotoEditType photoEditType, QString& newFileName)
{
    initializeSearchPaths();
    QDir dir(avatarDir());
    if (!dir.exists()) {
        return false;
    }
    QFileInfo fileInfo = QFileInfo(fullFilePath);
    int i = 1;
    if ( QFile::exists(fullFilePath) ) {
        if ( QFile::exists( avatarDir() + '/' + fileInfo.fileName() ) ) {
            do {
                newFileName = QString("%1(%2).%3").arg(fileInfo.baseName()).arg(QString::number(i,10)).arg(fileInfo.completeSuffix());
                i++;
            } while( QFile::exists(avatarDir() + '/' + newFileName) && i < 10000 );
        }
        else {
            newFileName = fileInfo.fileName();
        }
    }
    else {
        return false;
    }
    if ( photoEditType == ContactPhotoCopy::PhotoEditLeaveUntouched) {
        return QFile::copy(fullFilePath, avatarDir() + '/' + newFileName);
    }
    else {
        if (photoEditType == ContactPhotoCopy::PhotoEditCropAndScale &&
            scaledSize.height() != scaledSize.width()) {
            qDebug() << "\t" << Q_FUNC_INFO << "supports only square scaling when cropping";
        }
        QImage img;
        if (!img.load(fullFilePath)) {
            return false;
        }
        int originalWidth = img.width();
        int originalHeight = img.height();
        int newWidth = scaledSize.width();
        int newHeight = scaledSize.height();
        QImage scaledImg = img.scaled(newWidth, newHeight, Qt::KeepAspectRatioByExpanding);
        QImage croppedImg;
        if (originalWidth > originalHeight) {
            int left = (scaledImg.width() - scaledImg.height()) / 2;
            croppedImg = scaledImg.copy(left, 0, scaledImg.height(), scaledImg.height());
        }
        else if (originalWidth < originalHeight) {
            int top = (scaledImg.height() - scaledImg.width()) / 2;
            croppedImg = scaledImg.copy(0, top, scaledImg.width(), scaledImg.width());
        }
        else {
            croppedImg = scaledImg;
        }
        return croppedImg.save(avatarDir() + '/' + newFileName, 0, 85);
    }
}

void ContactPhotoCopy::removeFromAvatarDir(const QString &fileName)
{
    initializeSearchPaths();
    QFile::remove(avatarDir() + '/' + fileName);
}

QString ContactPhotoCopy::avatarDir()
{
    initializeSearchPaths();
    return avatarSearchPaths.at(0);
}

QStringList ContactPhotoCopy::searchPaths()
{
    initializeSearchPaths();
    return avatarSearchPaths;
}

void ContactPhotoCopy::initializeSearchPaths()
{
    if ( avatarSearchPaths.isEmpty() ) {
        avatarSearchPaths << QDir::homePath()  + "/.contacts/avatars"
                << QDir::rootPath() + "usr/share/contacts/themes/images"
                << QDir::rootPath() + "usr/share/themes/base/meegotouch/libcontactswidgets/svg";
        QString avatarPath = avatarSearchPaths.at(0);
        QDir dir(avatarPath);
        if (!dir.exists()) {
            if (!dir.mkpath(avatarPath)) {
                qDebug() << Q_FUNC_INFO << QString("Creating avatar cache directory \"%1\"failed").arg(avatarPath);
            }
        }
    }
}
