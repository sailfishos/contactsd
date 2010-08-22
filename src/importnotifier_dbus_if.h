/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

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
