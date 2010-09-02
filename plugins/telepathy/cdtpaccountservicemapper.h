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

#ifndef CDTPACCOUNTSERVICEMAPPER_H
#define CDTPACCOUNTSERVICEMAPPER_H

#include <QMap>
#include <QObject>

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
    QString serviceForAccountPath(const QString &accountPath) const;
private Q_SLOTS:
    void onAccountCreated(Accounts::AccountId id);
private:
    QList<ServiceMapData*> serviceMapData(const Accounts::Account *account) const;
    bool parseServiceXml(QXmlStreamReader *xml, ServiceMapData *serviceMapData) const;
    QMap<QString, QString> buildAccountServiceMap(const QMap<QString, QList<ServiceMapData*> > &serviceMap) const;
    const QString KeyUserName;
private:
    QMap<QString, QString> mAccountServiceMap;
    Accounts::Manager mAccountManager;
};

#endif // CDTPACCOUNTSERVICEMAPPER_H
