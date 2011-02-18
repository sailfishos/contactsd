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

#ifndef FAKEPLUGIN_H
#define FAKEPLUGIN_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

#include "base-plugin.h"

class FakePlugin : public Contactsd::BasePlugin
{
    Q_OBJECT

public:
    FakePlugin();
    ~FakePlugin();

    void init();
    MetaData metaData();
private Q_SLOTS:
    void timeout();
};

#endif // FAKEPLUGIN_H
