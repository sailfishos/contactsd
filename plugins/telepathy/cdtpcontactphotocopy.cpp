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

#include "cdtpcontactphotocopy.h"

#include <QDebug>
#include <QDir>
#include <QImage>

QStringList CDTpContactPhotoCopy::avatarSearchPaths;

bool CDTpContactPhotoCopy::copyToAvatarDir(const QString &fullFilePath,
        QString &newFileName)
{
    return CDTpContactPhotoCopy::copyToAvatarDir(fullFilePath, QSize(),
            CDTpContactPhotoCopy::PhotoEditLeaveUntouched, newFileName);
}

bool CDTpContactPhotoCopy::copyToAvatarDir(const QString& fullFilePath,
        const QSize& scaledSize, QString& newFileName)
{
    return CDTpContactPhotoCopy::copyToAvatarDir(fullFilePath, scaledSize,
            CDTpContactPhotoCopy::PhotoEditCropAndScale, newFileName);
}

bool CDTpContactPhotoCopy::copyToAvatarDir(const QString& fullFilePath,
        const QSize& scaledSize, PhotoEditType photoEditType, QString& newFileName)
{
    initializeSearchPaths();

    QDir dir(avatarDir());
    if (!dir.exists()) {
        return false;
    }

    QFileInfo fileInfo = QFileInfo(fullFilePath);
    int i = 1;
    if (QFile::exists(fullFilePath)) {
        // FIXME: When 10000 entry is reached, that file will be overwritten
        // subsequently
        newFileName = fileInfo.fileName();
        while (QFile::exists(dir.absoluteFilePath(newFileName)) && i < 10000) {
            newFileName = QString("%1(%2).%3").arg(fileInfo.baseName())
                    .arg(QString::number(i, 10)).arg(fileInfo.completeSuffix());
            i++;
        }
    } else {
        return false;
    }

    if (photoEditType == CDTpContactPhotoCopy::PhotoEditLeaveUntouched) {
        return QFile::copy(fullFilePath, dir.absoluteFilePath(newFileName));
    } else {
        if (photoEditType == CDTpContactPhotoCopy::PhotoEditCropAndScale &&
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
        QImage scaledImg = img.scaled(newWidth, newHeight,
                Qt::KeepAspectRatioByExpanding);
        QImage croppedImg;
        if (originalWidth > originalHeight) {
            int left = (scaledImg.width() - scaledImg.height()) / 2;
            croppedImg = scaledImg.copy(left, 0, scaledImg.height(), scaledImg.height());
        } else if (originalWidth < originalHeight) {
            int top = (scaledImg.height() - scaledImg.width()) / 2;
            croppedImg = scaledImg.copy(0, top, scaledImg.width(), scaledImg.width());
        } else {
            croppedImg = scaledImg;
        }
        return croppedImg.save(dir.absoluteFilePath(newFileName), 0, 85);
    }
}

void CDTpContactPhotoCopy::removeFromAvatarDir(const QString &fileName)
{
    initializeSearchPaths();
    QFile::remove(avatarDir() + '/' + fileName);
}

QString CDTpContactPhotoCopy::avatarDir()
{
    initializeSearchPaths();
    return avatarSearchPaths.at(0);
}

QStringList CDTpContactPhotoCopy::searchPaths()
{
    initializeSearchPaths();
    return avatarSearchPaths;
}

void CDTpContactPhotoCopy::initializeSearchPaths()
{
    if (avatarSearchPaths.isEmpty()) {
        avatarSearchPaths << QDir::homePath()  + "/.contacts/avatars" <<
            QDir::rootPath() + "usr/share/contacts/themes/images" <<
            QDir::rootPath() + "usr/share/themes/base/meegotouch/libcontactswidgets/svg";
        QString avatarPath = avatarSearchPaths.at(0);
        QDir dir(avatarPath);
        if (!dir.exists()) {
            if (!dir.mkpath(avatarPath)) {
                qDebug() << Q_FUNC_INFO <<
                    QString("Creating avatar cache directory \"%1\"failed")
                            .arg(avatarPath);
            }
        }
    }
}
