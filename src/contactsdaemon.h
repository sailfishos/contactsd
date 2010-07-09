/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */
#ifndef CONTACTSDAEMON_H_
#define CONTACTSDAEMON_H_
/* \class ContactsDaemon

   yayayayay

   */
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

#include "contactsdplugininterface.h"

class ContactsServiceAdaptor;

class ContactsDaemon : public QObject
{
    Q_OBJECT

public:
    explicit ContactsDaemon(QObject* const parent);
    virtual ~ContactsDaemon();

    void loadAllPlugins(const QStringList &loadPlugins = QStringList());
    const QStringList validPlugins() const;


signals:
    void pluginsLoaded();

private:
    class Private;
    Private * const d;
    ContactsServiceAdaptor* adaptor;
};

#endif
