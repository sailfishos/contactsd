/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact:  Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#include <QObject>

#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/ContactManager>

#include <pendingrosters.h>
#include <trackersink.h>
#include <telepathyaccount.h>
#include "accountservicemapper.h"

#include "contactsdplugininterface.h"

class TelepathyController;

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

private:
    void saveIMAccount(Tp::AccountPtr account, TelepathyAccount::Changes changes);
    bool saveAvatar(const QByteArray&, const QString&, const QString&, QString&);

    TelepathyController* m_tpController;
    TrackerSink* mStore;
    Tp::AccountManagerPtr mAm;
    QList<PendingRosters*> mRosters;
    QList<TelepathyAccount*> mAccounts;
    AccountServiceMapper accountServiceMapper;
};
