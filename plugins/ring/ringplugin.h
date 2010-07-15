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

// contactsd
#include <contactsdplugininterface.h>
// TelepathyQt4
#include <TelepathyQt4/Types>
// Qt
#include <QObject>

namespace Tp
{
    class PendingOperation;
}
class QSettings;


class RingPlugin : public QObject, public ContactsdPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(ContactsdPluginInterface)
public:
    RingPlugin();
    ~RingPlugin();
    void init();
    QMap<QString, QVariant> metaData();

private Q_SLOTS:
    void onFinished(Tp::PendingOperation* op);

private:
    Tp::AccountManagerPtr  mAccountManger;
    QSettings* mCache;
};
