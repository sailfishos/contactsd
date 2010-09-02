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

#include "cdtpaccountservicemapper.h"

#include <QDebug>
#include <QUrl>
#include <QXmlStreamReader>

class ServiceMapData
{
public:
    ServiceMapData(const QString &serviceName) : serviceName(serviceName)
    {
    }

    bool isEmpty() const
    {
        return (manager.isEmpty() || protocol.isEmpty());
    }

    QString manager;
    QString protocol;
    QString serviceName;
};

AccountServiceMapper::AccountServiceMapper(QObject *parent)
    : QObject(parent),
      KeyUserName(QLatin1String("username"))
{
    connect(&mAccountManager, SIGNAL(accountCreated(Accounts::AccountId)),
            this, SLOT(onAccountCreated(Accounts::AccountId)));
}

AccountServiceMapper::~AccountServiceMapper()
{
}

void AccountServiceMapper::initialize()
{
    const Accounts::AccountIdList accounts = mAccountManager.accountList();
    QMap<QString, QList<ServiceMapData*> > serviceMap;
    foreach (const Accounts::AccountId &id, accounts) {
        Accounts::Account *account = mAccountManager.account(id);
        QList<ServiceMapData*> data = serviceMapData(account);
        if (data.count() > 0) {
            serviceMap.insert(account->valueAsString(KeyUserName, QString()), data);
        }
    }

    mAccountServiceMap = buildAccountServiceMap(serviceMap);

    foreach (QList<ServiceMapData*> toDelete, serviceMap.values()) {
        qDeleteAll(toDelete.begin(), toDelete.end());
    }
    serviceMap.clear();
}

QList<ServiceMapData*> AccountServiceMapper::serviceMapData(const Accounts::Account *account) const
{
    QList<ServiceMapData*> result;
    const Accounts::ServiceList services = account->services();
    foreach (const Accounts::Service *service, services) {
        ServiceMapData *serviceMapData = new ServiceMapData(service->name());
        parseServiceXml(service->xmlStreamReader(), serviceMapData);
        if (serviceMapData->isEmpty()) {
            delete serviceMapData;
        } else {
            result.append(serviceMapData);
        }
    }
    return result;
}

QString AccountServiceMapper::serviceForAccountPath(const QString &accountPath) const
{
    foreach (const QString &key, mAccountServiceMap.keys()) {
        QString accountPathCopy(accountPath);
        accountPathCopy.chop(1); // Remove possible trailing account index number
        if (accountPathCopy.endsWith(key, Qt::CaseInsensitive)) {
            return mAccountServiceMap.value(key);
        }
    }
    return QString();
}

void AccountServiceMapper::onAccountCreated(Accounts::AccountId id)
{
    QMap<QString, QList<ServiceMapData*> > serviceMap;
    Accounts::Account *account = mAccountManager.account(id);
    QList<ServiceMapData*> data = serviceMapData(account);
    if (data.count() > 0) {
        serviceMap.insert(account->valueAsString(KeyUserName, QString()), data);
    }

    QMap<QString, QString> accountServiceMap = buildAccountServiceMap(serviceMap);
    foreach (const QString &partialAccountPath, accountServiceMap.keys()) {
        if (mAccountServiceMap.contains(partialAccountPath)) {
            qWarning() << Q_FUNC_INFO << "New account created, but it already exists in mapping";
        } else {
            mAccountServiceMap.insert(partialAccountPath, accountServiceMap.value(partialAccountPath));
        }
    }

    foreach (QList<ServiceMapData*> toDelete, serviceMap.values()) {
        qDeleteAll(toDelete.begin(), toDelete.end());
    }
    serviceMap.clear();
}

bool AccountServiceMapper::parseServiceXml(QXmlStreamReader *xml, ServiceMapData *serviceMapData) const
{
    if (!xml) {
        return false;
    }
    bool ok = false;
    while (!xml->atEnd()) {
        bool start = xml->readNextStartElement();
        if (start && xml->name() == QLatin1String("template")) {
            ok = true;
            break;
        }
    }
    QString manager;
    QString protocol;
    if (ok) {
        while (!xml->atEnd()) {
           bool start = xml->readNextStartElement();
           if (start && xml->name() == QLatin1String("setting")) {
               QXmlStreamAttributes attributes = xml->attributes();
               QStringRef attr = attributes.value(QLatin1String("name"));
               if (QLatin1String("manager") == attr) {
                   manager = xml->readElementText();
               } else if (QLatin1String("protocol") == attr) {
                   protocol = xml->readElementText();
               }
           }
           if (!manager.isEmpty() && !protocol.isEmpty()) {
               break;
           }
        }
    }
    serviceMapData->manager = manager;
    serviceMapData->protocol = protocol;
    if (xml->hasError()) {
        qWarning() << "Error reading service xml file for service" << serviceMapData->serviceName;
    }
    return xml->hasError();
}

QMap<QString, QString>
AccountServiceMapper::buildAccountServiceMap(const QMap<QString, QList<ServiceMapData*> > &serviceMap) const
{
    QMap<QString, QString> accountServiceMapTemp;
    foreach (const QString &account, serviceMap.keys()) {
        QList<ServiceMapData*> mapData = serviceMap.value(account);
        foreach (const ServiceMapData *data, mapData) {
            if (data) {
            // TODO Find out which characters need to be encoded
                QString percentEncodedAccount = QString(QUrl::toPercentEncoding(account, QByteArray(), "._-"));
                const QString percentReplacedAccount = percentEncodedAccount.replace(QChar('%'), QChar('_'));
                const QString partialAccountPath = QString(QLatin1String("/%1/%2/%3"))
                                                   .arg(data->manager)
                                                   .arg(data->protocol)
                                                   .arg(percentReplacedAccount);
                // TODO check duplicates
                accountServiceMapTemp.insert(partialAccountPath, data->serviceName);
            }
        }
    }
    return accountServiceMapTemp;
}
