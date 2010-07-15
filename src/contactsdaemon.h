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

#ifndef CONTACTSDAEMON_H_
#define CONTACTSDAEMON_H_
/* \class ContactsDaemon

   yayayayay

   */

// Qt
#include <QObject>
#include <QStringList>

class ContactsServiceAdaptor;


class ContactsDaemon : public QObject
{
    Q_OBJECT

public:
    explicit ContactsDaemon(QObject* const parent);
    virtual ~ContactsDaemon();

    void loadAllPlugins(const QStringList &loadPlugins = QStringList());
    QStringList validPlugins() const;

Q_SIGNALS:
    void pluginsLoaded();

private:
    class Private;
    Private * const d;
    ContactsServiceAdaptor* adaptor;
};

#endif
