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

#include <QtSparql>
#include <cubi.h>

#include "cdtpaccount.h"
#include "cdtpcontact.h"

CUBI_USE_NAMESPACE

class CDTpQueryBuilder
{
public:
    CDTpQueryBuilder();

    void append(const UpdateBase &q);
    void prepend(const UpdateBase &q);

    void append(const CDTpQueryBuilder &builder);
    void prepend(const CDTpQueryBuilder &builder);

    QString sparql(Options::SparqlOptions options = Options::DefaultSparqlOptions) const;
    QSparqlQuery sparqlQuery() const;

private:
    QList<UpdateBase> mQueries;
};

/* --- CDTpSparqlQuery --- */

class CDTpSparqlQuery : public QObject
{
    Q_OBJECT

public:
    CDTpSparqlQuery(const CDTpQueryBuilder &builder, QObject *parent = 0);
    ~CDTpSparqlQuery() {};

    bool hasError() { return mErrorSet; }
    QSparqlError error() { return mError; }

Q_SIGNALS:
    void finished(CDTpSparqlQuery *query);

private Q_SLOTS:
    void onQueryFinished();

private:
    uint mId;
    QTime mTime;
    bool mErrorSet;
    QSparqlError mError;
};

/* --- CDTpAccountsSparqlQuery --- */

class CDTpAccountsSparqlQuery : public CDTpSparqlQuery
{
    Q_OBJECT

public:
    CDTpAccountsSparqlQuery(const QList<CDTpAccountPtr> &accounts,
        const CDTpQueryBuilder &builder, QObject *parent = 0);
    CDTpAccountsSparqlQuery(const CDTpAccountPtr &accountWrapper,
        const CDTpQueryBuilder &builder, QObject *parent = 0);
    ~CDTpAccountsSparqlQuery() {};

    QList<CDTpAccountPtr> accounts() const { return mAccounts; };

private:
    QList<CDTpAccountPtr> mAccounts;
};


#endif // CDTPQUERY_H
