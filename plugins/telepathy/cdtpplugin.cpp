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

#include "cdtpplugin.h"

#include "cdtpcontroller.h"

#include <TelepathyQt4/Debug>

CDTpPlugin::CDTpPlugin()
    : mController(0)
{
}

CDTpPlugin::~CDTpPlugin()
{
}

void CDTpPlugin::init()
{
    qDebug() << "Initializing contactsd telepathy plugin";

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    qDebug() << "Creating controller";
    mController = new CDTpController(this);
    // relay signals
    connect(mController,
            SIGNAL(importStarted()),
            SIGNAL(importStarted()));
    connect(mController,
            SIGNAL(importEnded(int, int, int)),
            SIGNAL(importEnded(int, int, int)));
}

QMap<QString, QVariant> CDTpPlugin::metaData()
{
    QMap<QString, QVariant> data;
    data["name"] = QVariant(QString("telepathy"));
    data["version"] = QVariant(QString("0.1"));
    data["comment"] = QVariant(QString("contactsd telepathy plugin"));
    return data;
}

bool CDTpPlugin::hasActiveImports() const
{
    return mController ? mController->hasActiveImports() :  false;
}

Q_EXPORT_PLUGIN2(CDTpPlugin, CDTpPlugin)
