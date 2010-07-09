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

#include "ringplugin.h"

#include "telepathycontroller.h"

RingPlugin::RingPlugin()
    : mCache(0)
{
}

RingPlugin::~RingPlugin()
{
  delete mCache;
}

void RingPlugin::init()
{
    mCache = new QSettings(QSettings::IniFormat, QSettings::UserScope, "Nokia",
                           "ContactsRingCache");
    mAccountManger = Tp::AccountManager::create();
    connect(mAccountManger->becomeReady(), SIGNAL(finished(Tp::PendingOperation*)),
            this,SLOT(onFinished(Tp::PendingOperation*)));

}

void RingPlugin::onFinished(Tp::PendingOperation* op)
{
    if (op->isError()) {
        return;
    }

    foreach (Tp::AccountPtr account, mAccountManger->validAccounts()) {
        if (account->cmName() == "ring" ) {
            QString mRingAccountPath = account->objectPath();
            QString storedPath = mCache->value("AccountPath", "").toString();
            if (storedPath != mRingAccountPath) {
                mCache->setValue("AccountPath",mRingAccountPath);
                mCache->sync();
            }
        }
    }
    qDebug() << Q_FUNC_INFO << ": Request finished. Account path is: " << mCache->value("AccountPath");

}

QMap<QString, QVariant> RingPlugin::metaData()
{
    QMap<QString, QVariant> data;
    data["name"]= QVariant(QString("ring"));
    data["version"] = QVariant(QString("0.1"));
    data["comment"] = QVariant(QString("Keeps the ring path sync for making calls"));
    //TODO: translations ?
    return data;
}
Q_EXPORT_PLUGIN2(RgPlugin, RingPlugin)
