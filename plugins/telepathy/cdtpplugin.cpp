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


#include <TelepathyQt4/Debug>

#include "cdtpcontroller.h"
#include "cdtpplugin.h"
#include "debug.h"

using namespace Contactsd;

CDTpPlugin::CDTpPlugin()
    : mController(0)
{
}

CDTpPlugin::~CDTpPlugin()
{
    if (mController) {
        delete mController;
    }
}

void CDTpPlugin::init()
{
    debug() << "Initializing contactsd telepathy plugin";

    Tp::registerTypes();
    Tp::enableDebug(isDebugEnabled());
    Tp::enableWarnings(isWarningsEnabled());

    debug() << "Creating controller";
    mController = new CDTpController(this);
    // relay signals
    connect(mController,
            SIGNAL(importStarted(const QString &, const QString &)),
            SIGNAL(importStarted(const QString &, const QString &)));
    connect(mController,
            SIGNAL(importEnded(const QString &, const QString &, int, int, int)),
            SIGNAL(importEnded(const QString &, const QString &, int, int, int)));
    connect(mController,
            SIGNAL(error(int, const QString &)),
            SIGNAL(error(int, const QString &)));
}

CDTpPlugin::MetaData CDTpPlugin::metaData()
{
    MetaData data;
    data[metaDataKeyName]    = QVariant(QString::fromLatin1("telepathy"));
    data[metaDataKeyVersion] = QVariant(QString::fromLatin1("0.2"));
    data[metaDataKeyComment] = QVariant(QString::fromLatin1("contactsd telepathy plugin"));
    return data;
}

Q_EXPORT_PLUGIN2(telepathyplugin, CDTpPlugin)
