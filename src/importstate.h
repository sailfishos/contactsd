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

#ifndef IMPORTSTATE_H_
#define IMPORTSTATE_H_

#include <QMultiHash>
#include <QString>
#include <QStringList>

class ImportState
{
public:
    ImportState();

    bool hasActiveImports();
    // reset the state and return true only when there's no active import
    // otherwise it will fail and return false.
    bool reset();

    // check if a service still has active importing accounts
    bool serviceHasActiveImports(const QString &service);

    void addImportingAccount(const QString &service, const QString &account);
    void removeImportingAccount(const QString &service, const QString &account,
                                int added, int removed, int merged);

    int contactsAdded();
    int contactsMerged();
    int contactsRemoved();

private:
    // each service may have multiple active importing accounts
    QMultiHash<QString, QString> mService2Accounts;

    // accumlated amount of contacts being added, merged, removed
    int mContactsAdded;
    int mContactsMerged;
    int mContactsRemoved;
};

#endif // IMPORTSTATE_H_
