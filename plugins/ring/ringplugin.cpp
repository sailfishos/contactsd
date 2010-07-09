/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

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
