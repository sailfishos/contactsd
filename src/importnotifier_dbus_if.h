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


#ifndef IMPORTNOTIFIER_H_
#define IMPORTNOTIFIER_H_


#include <QObject>

/**
 * This is the DBus interface that ImportNotifierAdapter implements and that
 * ImportNotifierInterface provides. This file is used for generating the 
 * DBus classes with qdbusxml2cpp and qdbuscpp2xml.
 */
class ImportNotifier : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.nokia.contacts.importprogress");

public:
    ImportNotifier();

public Q_SLOTS:
    bool hasActiveImports();


Q_SIGNALS:
    void importStarted();
    void importEnded(int contactsAdded, int contactsRemoved, int contactsMerged);

};
#endif
