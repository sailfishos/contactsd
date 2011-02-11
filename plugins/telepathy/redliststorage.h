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

#ifndef REDLISTSTORAGE_H
#define REDLISTSTORAGE_H

#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QString>

#include "cdtpcontact.h"


namespace com {
namespace nokia {
namespace contactsd {

class RedListStoragePrivate;

typedef QHash<QString, QSet <QString> > IdsOnAccountPaths;

class RedListStorage
{
public:
    RedListStorage();

    const IdsOnAccountPaths &idsToBeRemoved() const;
    void addIdsToBeRemoved(const IdsOnAccountPaths &idsForRemoval);
    void clearIdsToBeRemoved(const IdsOnAccountPaths &idsForRemoval);
    void processRosterListChange(const QString accountObjectPath,
                                 QList<CDTpContactPtr> &contactsAdded,
                                 QList<CDTpContactPtr> &contactsRemoved);

private:
    void flash();

private:
    RedListStoragePrivate *d_ptr;
    Q_DISABLE_COPY(RedListStorage);
    Q_DECLARE_PRIVATE(RedListStorage);
};

}
}
}

#endif // REDLISTSTORAGE_H
