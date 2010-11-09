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

namespace Tp
{
    class PendingOperation;
}

class CDTpController : public QObject
{
    Q_OBJECT

public:
    CDTpController(QObject *parent = 0);
    ~CDTpController();

    bool hasActiveImports() const;

Q_SIGNALS:
    void importStarted(const QString &service, const QString &account);
    void importEnded(const QString &service, const QString &account, int contactsAdded, int contactsRemoved, int contactsMerged);

private Q_SLOTS:
    void onAccountManagerReady(Tp::PendingOperation *op);
    void onAccountAdded(const Tp::AccountPtr &account);
    // FIXME Tp::AccountSet is deleting the account before emitting the signal
    // void onAccountRemoved(const Tp::AccountPtr &account);
    void onAccountRemoved(const QString &account);
    void onAccountRosterChanged(CDTpAccount *accountWrapper, bool haveRoster);
    void onAccountRosterUpdated(CDTpAccount *accountWrapper,
            const QList<CDTpContactPtr> &contactsAdded,
            const QList<CDTpContactPtr> &contactsRemoved);

private:
    void insertAccount(const Tp::AccountPtr &account);
    void removeAccount(const QString &accountObjectPath);

    void setImportStarted(const Tp::AccountPtr &account);
    void setImportEnded(const Tp::AccountPtr &account,
                        int contactsAdded, int contactsRemoved);

    CDTpStorage *mStorage;
    Tp::AccountManagerPtr mAM;
    Tp::AccountSetPtr mAccountSet;
    QHash<QString, CDTpAccount *> mAccounts;
    bool mImportActive;
};

#endif // CDTPCONTROLLER_H
