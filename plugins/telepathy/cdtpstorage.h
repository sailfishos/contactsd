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

#ifndef CDTPSTORAGE_H
#define CDTPSTORAGE_H

#include "cdtpaccount.h"
#include "cdtpcontact.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nie.h>
#include <QtTracker/QLive>
#include <QtTracker/Tracker>

using namespace SopranoLive;

class CDTpStorage : public QObject
{
public:
    CDTpStorage(QObject *parent = 0);
    ~CDTpStorage();

public Q_SLOTS:
    void syncAccount(CDTpAccount *accountWrapper);
    void syncAccount(CDTpAccount *accountWrapper, CDTpAccount::Changes changes);
    void syncAccountContacts(CDTpAccount *accountWrapper);
    void syncAccountContacts(CDTpAccount *accountWrapper,
            const QList<CDTpContact *> &contactsAdded,
            const QList<CDTpContact *> &contactsRemoved);
    void syncAccountContact(CDTpAccount *accountWrapper,
            CDTpContact *contactWrapper, CDTpContact::Changes changes);
    void setAccountContactsOffline(CDTpAccount *accountWrapper);
    void removeAccount(CDTpAccount *accountWrapper);

private:
    bool saveAccountAvatar(const QByteArray &data, const QString &mimeType,
            const QString &path, QString &fileName);

    void addContactAliasInfoToQuery(RDFUpdate &query,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);
    void addContactPresenceInfoToQuery(RDFUpdate &query,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);
    void addContactCapabilitiesInfoToQuery(RDFUpdate &query,
            const RDFVariable &imAddress,
            CDTpContact *contactWrapper);

    QString contactLocalId(const QString &contactAccountObjectPath,
            const QString &contactId) const;
    QString contactLocalId(CDTpContact *contactWrapper) const;

    QUrl contactIri(const QString &contactLocalId) const;
    QUrl contactIri(CDTpContact *contactWrapper) const;

    QUrl contactImAddress(const QString &contactAccountObjectPath,
            const QString &contactId) const;
    QUrl contactImAddress(CDTpContact *contactWrapper) const;

    QUrl trackerStatusFromTpPresenceType(uint tpPresenceType) const;
    QUrl trackerStatusFromTpPresenceStatus(
            const QString &tpPresenceStatus) const;
    void updateAvatar(RDFUpdate &query, const QUrl &url,
            const QUrl &fileName,
            bool deleteOnly);
};

#endif // CDTPSTORAGE_H
