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

#ifndef CDTPCONTROLLER_H
#define CDTPCONTROLLER_H

#include "cdtpaccount.h"
#include "cdtpcontact.h"
#include "cdtpstorage.h"

#include <TelepathyQt4/Types>

#include <QList>
#include <QObject>

class CDTpController : public QObject
{
    Q_OBJECT

public:
    CDTpController(QObject *parent = 0);
    ~CDTpController();

Q_SIGNALS:
    void importStarted(const QString &service, const QString &account);
    void importEnded(const QString &service, const QString &account, int contactsAdded, int contactsRemoved, int contactsMerged);

private Q_SLOTS:
    void onAccountManagerReady(Tp::PendingOperation *op);
    void onAccountAdded(const Tp::AccountPtr &account);
    void onAccountRemoved(const Tp::AccountPtr &account);
    void onAccountReady(CDTpAccountPtr accountWrapper);
    void onSyncStarted(CDTpAccountPtr accountWrapper);
    void onSyncEnded(CDTpAccountPtr accountWrapper, int contactsAdded, int contactsRemoved);

private:
    CDTpAccountPtr insertAccount(const Tp::AccountPtr &account);
    void removeAccount(const QString &accountObjectPath);

    void setImportStarted(const Tp::AccountPtr &account);
    void setImportEnded(const Tp::AccountPtr &account,
                        int contactsAdded, int contactsRemoved);

    CDTpStorage *mStorage;
    Tp::AccountManagerPtr mAM;
    Tp::AccountSetPtr mAccountSet;
    QHash<Tp::AccountPtr, CDTpAccountPtr> mAccounts;
};

#endif // CDTPCONTROLLER_H
