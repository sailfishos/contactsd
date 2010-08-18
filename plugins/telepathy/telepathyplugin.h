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

// this plugin
#include "accountservicemapper.h"
// contactsd
#include <contactsdplugininterface.h>
// telepathy support
#include <telepathyaccount.h>
// TelepathyQt4
#include <TelepathyQt4/Types>
// Qt
#include <QObject>

class TrackerSink;
class PendingRosters;
class TelepathyController;
class TelepathyAccount;
namespace Tp
{
    class PendingOperation;
}


class TelepathyPlugin : public QObject, public ContactsdPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(ContactsdPluginInterface)

public:
    TelepathyPlugin();
    ~TelepathyPlugin();

    void init();
    QMap<QString, QVariant> metaData();
    void saveSelfContact(Tp::AccountPtr);

Q_SIGNALS:
    void error(const QString &, const QString &, const QString &);

private Q_SLOTS:
    void onFinished(Tp::PendingOperation* op);
    void onAccountManagerReady(Tp::PendingOperation*);
    void onAccountCreated(const QString &);
    void onAccountReady(Tp::PendingOperation*);
    void onAccountChanged(TelepathyAccount*, TelepathyAccount::Changes changes);
    void accountModelReady(Tp::AccountPtr);
    void onAccountRemoved();
    void onOnlinenessChanged(bool online);
    void onConnectionChanged(bool connection);
    void onContactsChanged(const Tp::Contacts& added, const Tp::Contacts& removed);

private:
    void saveIMAccount(Tp::AccountPtr account, TelepathyAccount::Changes changes);
    bool saveAvatar(const QByteArray&, const QString&, const QString&, QString&);

    TelepathyController* m_tpController;
    TrackerSink* mStore;
    Tp::AccountManagerPtr mAm;
    QList<PendingRosters*> mRosters;
    QList<TelepathyAccount*> mAccounts;
    QList<Tp::ConnectionPtr> mConnections;
    AccountServiceMapper accountServiceMapper;
};
