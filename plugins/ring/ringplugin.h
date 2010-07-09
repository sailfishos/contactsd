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

#include <QObject>

#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/ContactManager>
#include <QSettings>

#include "contactsdplugininterface.h"

class TelepathyController;

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
