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

#include <contactsdplugininterface.h>

#include "cdtpaccountservicemapper.h"
#include "cdtpaccount.h"
#include "cdtpcontact.h"

#include <TelepathyQt4/Types>

#include <QObject>

class CDTpTrackerSink;
class CDTpPendingRosters;
class CDTpController;
class CDTpAccount;
namespace Tp
{
    class PendingOperation;
}


class CDTpPlugin : public QObject, public ContactsdPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(ContactsdPluginInterface)

public:
    CDTpPlugin();
    ~CDTpPlugin();

    void init();
    QMap<QString, QVariant> metaData();
    void saveSelfContact(Tp::AccountPtr);

Q_SIGNALS:
    void error(const QString &, const QString &, const QString &);
    void importStarted();
    void importEnded(int contactsAdded, int contactsRemoved, int contactsMerged);

private Q_SLOTS:
    void onFinished(CDTpPendingRosters* op);
    void onAccountManagerReady(Tp::PendingOperation*);
    void onAccountCreated(const QString &);
    void onAccountReady(Tp::PendingOperation*);
    void onAccountChanged(CDTpAccount*, CDTpAccount::Changes changes);
    void accountModelReady(Tp::AccountPtr);
    void onAccountRemoved();
    void onOnlinenessChanged(bool online);
    void onConnectionChanged(bool connection);
    void onContactsChanged(const Tp::Contacts& added, const Tp::Contacts& removed);
    void onContactsRemoved(QList<QSharedPointer<CDTpContact> > list);
    void onContactsAdded(QList<QSharedPointer<CDTpContact> > list);
    bool hasActiveImports();

private:
    void saveIMAccount(Tp::AccountPtr account, CDTpAccount::Changes changes);
    bool saveAvatar(const QByteArray&, const QString&, const QString&, QString&);

    CDTpController* m_tpController;
    CDTpTrackerSink* mStore;
    Tp::AccountManagerPtr mAm;
    QList<CDTpPendingRosters*> mRosters;
    QList<CDTpAccount*> mAccounts;
    QList<Tp::ConnectionPtr> mConnections;
    CDTpAccountServiceMapper accountServiceMapper;
    bool mImportActive;
};

#endif // CDTPPLUGIN_H
