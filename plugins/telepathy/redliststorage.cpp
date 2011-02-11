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

#include <QtCore/QSettings>

#include "redliststorage.h"

namespace com {
namespace nokia {
namespace contactsd {

const QLatin1String Account("account");
const QLatin1String Removed("removed");
const QLatin1String RedList("redList");
const QLatin1String Separator(",");


class RedListStoragePrivate {
public:
    RedListStoragePrivate()
        : settings(QSettings::IniFormat, QSettings::UserScope, QLatin1String("Nokia"),
              QLatin1String("Contactsd"))
    {

    }

public:
    QSettings settings;
    IdsOnAccountPaths idsForRemoval;
};

RedListStorage::RedListStorage()
    : d_ptr(new RedListStoragePrivate)
{
    int size = d_func()->settings.beginReadArray(RedList);
    for (int i = 0; i < size; ++i) {
        d_func()->settings.setArrayIndex(i);
        QSet< QString > ids = d_func()->settings.value(Account).toString().split(Separator).toSet();
        if (not ids.isEmpty()) {
            d_func()->idsForRemoval[d_func()->settings.value(Account).toString()] = ids;
        }
    }
    d_func()->settings.endArray();
}

const IdsOnAccountPaths &RedListStorage::idsToBeRemoved() const
{
    return d_func()->idsForRemoval;
}

void RedListStorage::addIdsToBeRemoved(const IdsOnAccountPaths &idsForRemoval)
{
    for (IdsOnAccountPaths::ConstIterator it = idsForRemoval.constBegin(); idsForRemoval.constEnd() != it; it++) {
        QSet<QString> &idListForAccount(d_func()->idsForRemoval[it.key()]);
        idListForAccount += it.value();
    }
    flash();
}

void RedListStorage::clearIdsToBeRemoved(const IdsOnAccountPaths &idsForRemoval)
{
    for (IdsOnAccountPaths::ConstIterator it = idsForRemoval.constBegin(); idsForRemoval.constEnd() != it; it++) {
        QSet<QString> &idListForAccount(d_func()->idsForRemoval[it.key()]);
        idListForAccount -= it.value();
    }
    flash();
}

void RedListStorage::processRosterListChange(const QString accountObjectPath,
                             QList<CDTpContactPtr> &contactsAdded,
                             QList<CDTpContactPtr> &contactsRemoved)
{
    // FIXME implement (using contactsAdded) that invited contact is not displayed as duplicate in UI
    Q_UNUSED(contactsAdded)
    bool dirty(false);

    QList<CDTpContactPtr> contactsRemovedProcessed;
    if (not d_func()->idsForRemoval.contains(accountObjectPath)) {
        return;
    }

    QSet<QString> &idsRemoveForAccount(d_func()->idsForRemoval[accountObjectPath]);
    if (idsRemoveForAccount.isEmpty()) {
        return;
    }
    /* Remove those already removed */
    Q_FOREACH (const CDTpContactPtr &contactPtr, contactsRemovedProcessed) {
        if (not idsRemoveForAccount.remove(contactPtr->contact()->id())) {
            contactsRemovedProcessed << contactPtr;
        } else {
            dirty = true;
        }
    }
    contactsRemoved = contactsRemovedProcessed;
    if (dirty) {
        flash();
    }
}

void RedListStorage::flash()
{
    const QString redListSection(RedList);
    d_func()->settings.beginWriteArray(redListSection);
    int index(0);
    for (IdsOnAccountPaths::ConstIterator it = d_func()->idsForRemoval.constBegin(); d_func()->idsForRemoval.constEnd() != it; it++) {
        d_func()->settings.setArrayIndex(++index);
        const QSet<QString> &idListForAccount(d_func()->idsForRemoval[it.key()]);
        d_func()->settings.setValue(Account, it.key());
        d_func()->settings.setValue(Removed, QStringList(idListForAccount.toList()).join(Separator));
    }
    d_func()->settings.endArray();
}

}
}
}
