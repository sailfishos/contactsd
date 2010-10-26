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

#ifndef CDTPPLUGIN_H
#define CDTPPLUGIN_H

#include <ContactsdPluginInterface>

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

class CDTpController;

class CDTpPlugin : public QObject, public ContactsdPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(ContactsdPluginInterface)

public:
    CDTpPlugin();
    ~CDTpPlugin();

    void init();
    QMap<QString, QVariant> metaData();
    bool hasActiveImports() const;

Q_SIGNALS:
    void importStarted(const QString &service, const QString &account);
    void importEnded(const QString &service, const QString &account,
                     int contactsAdded, int contactsRemoved, int contactsMerged);

private:
    CDTpController *mController;
};

#endif // CDTPPLUGIN_H
