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

#ifndef CDTPQUERY_H
#define CDTPQUERY_H

#include <QObject>

#include <QtTracker/Tracker>

#include "cdtpaccount.h"
#include "cdtpcontact.h"

using namespace SopranoLive;

/* --- CDTpSelectQuery --- */

class CDTpSelectQuery : public QObject
{
    Q_OBJECT

public:
    CDTpSelectQuery(const RDFSelect &select, QObject *parent = 0);
    ~CDTpSelectQuery() {};

    LiveNodes reply() const { return mReply; }

Q_SIGNALS:
    void finished(CDTpSelectQuery *query);

protected:
    CDTpSelectQuery(QObject *parent = 0);
    void setSelect(const RDFSelect &select);

private Q_SLOTS:
    void onModelUpdated();

private:
    LiveNodes mReply;
};

/* --- CDTpContactsSelectQuery --- */

class CDTpContactsSelectItem
{
public:
    QString imContact;
    QString imAddress;
    QString generator;
    QString localUID;
};

class CDTpContactsSelectQuery: public CDTpSelectQuery
{
    Q_OBJECT

public:
    CDTpContactsSelectQuery(const QList<CDTpContactPtr> &contacts, QObject *parent = 0);
    CDTpContactsSelectQuery(const QString &accountObjectPath, QObject *parent = 0);
    ~CDTpContactsSelectQuery() {};

    QList<CDTpContactsSelectItem> items();

protected:
    CDTpContactsSelectQuery(QObject *parent = 0);
    void setSelect(const RDFVariable &imContact, const RDFVariable &imAddress);
    void setSelect(const QList<CDTpContactPtr> &contacts);

private:
    void ensureParsed();

    bool mReplyParsed;
    QList<CDTpContactsSelectItem> mItems;
};

/* --- CDTpAccountContactsSelectQuery --- */

class CDTpAccountContactsSelectQuery: public CDTpContactsSelectQuery
{
    Q_OBJECT

public:
    CDTpAccountContactsSelectQuery(CDTpAccountPtr accountWrapper, QObject *parent = 0);
    ~CDTpAccountContactsSelectQuery() {};

    CDTpAccountPtr accountWrapper() const { return mAccountWrapper; };

private:
    CDTpAccountPtr mAccountWrapper;
};

/* --- CDTpContactResolver --- */

class CDTpContactResolver : public CDTpContactsSelectQuery
{
    Q_OBJECT

public:
    CDTpContactResolver(
            const QHash<CDTpContactPtr, CDTpContact::Changes> &contactsToResolve,
            QObject *parent = 0);
    ~CDTpContactResolver() {};

    QList<CDTpContactPtr> remoteContacts() const;
    CDTpContact::Changes contactChanges(const CDTpContactPtr &contactWrapper) const;
    QString storageIdForContact(const CDTpContactPtr &contactWrapper);

private:
    void ensureParsed();

    bool mReplyParsed;
    QHash<CDTpContactPtr, CDTpContact::Changes> mContacts;
    QHash<CDTpContactPtr, QString> mResolvedContacts;
};

/* --- CDTpUpdateQuery --- */

class CDTpUpdateQuery : public QObject
{
    Q_OBJECT

public:
    CDTpUpdateQuery(RDFUpdate &updateQuery, QObject *parent = 0);
    ~CDTpUpdateQuery() {};

Q_SIGNALS:
    void finished(CDTpUpdateQuery *query);

private Q_SLOTS:
    void onCommitFinished();
    void onCommitError(QString message);

private:
    QString mSparql;
    RDFTransactionPtr mTransaction;
};


/* --- CDTpAccountsUpdateQuery --- */

class CDTpAccountsUpdateQuery : public CDTpUpdateQuery
{
    Q_OBJECT

public:
    CDTpAccountsUpdateQuery(const QList<CDTpAccountPtr> &accounts,
        RDFUpdate &updateQuery, QObject *parent = 0);
    ~CDTpAccountsUpdateQuery() {};

    QList<CDTpAccountPtr> accounts() const { return mAccounts; };

private:
    QList<CDTpAccountPtr> mAccounts;
};

#endif // CDTPQUERY_H
