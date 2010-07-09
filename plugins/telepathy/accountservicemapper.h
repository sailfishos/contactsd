/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#ifndef ACCOUNTSERVICEMAPPER_H
#define ACCOUNTSERVICEMAPPER_H

#include <QObject>
#include <QMap>
#include <accounts-qt/manager.h>

class QXmlStreamReader;
class ServiceMapData;

class AccountServiceMapper : public QObject
{
    Q_OBJECT
public:
    AccountServiceMapper(QObject *parent = 0);
    virtual ~AccountServiceMapper();
    void initialize();
    QString serviceForAccountPath(const QString &accountPath);
private Q_SLOTS:
    void onAccountCreated(Accounts::AccountId id);
private:
    QList<ServiceMapData*> serviceMapData(const Accounts::Account *account);
    bool parseServiceXml(QXmlStreamReader *xml, ServiceMapData *serviceMapData);
    QMap<QString, QString> buildAccountServiceMap(const QMap<QString, QList<ServiceMapData*> > &serviceMap) const;
    const QString KeyUserName;
private:
    QMap<QString, QString> mAccountServiceMap;
    Accounts::Manager mAccountManager;
};

#endif
