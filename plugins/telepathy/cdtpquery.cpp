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

#include "cdtpquery.h"
#include "cdtpstorage.h"
#include "debug.h"
#include "sparqlconnectionmanager.h"

using namespace Contactsd;

/* --- CDTpQueryBuilder --- */

CDTpQueryBuilder::CDTpQueryBuilder()
{
}

void CDTpQueryBuilder::append(const UpdateBase &q)
{
    mQueries.append(q);
}

void CDTpQueryBuilder::prepend(const UpdateBase &q)
{
    mQueries.prepend(q);
}

void CDTpQueryBuilder::append(const CDTpQueryBuilder &builder)
{
    mQueries.append(builder.mQueries);
}

void CDTpQueryBuilder::prepend(const CDTpQueryBuilder &builder)
{
    // Poor man's QList::prepend - QTBUG-4312
    QList<UpdateBase> tmp;
    tmp.append(builder.mQueries);
    tmp.append(mQueries);
    mQueries = tmp;
}

QString CDTpQueryBuilder::sparql(Options::SparqlOptions options) const
{
    QString rawQuery;
    Q_FOREACH (const UpdateBase &q, mQueries) {
        rawQuery += q.sparql(options);
        rawQuery += QLatin1String("\n");
    }

    return rawQuery;
}

QSparqlQuery CDTpQueryBuilder::sparqlQuery() const
{
    return QSparqlQuery(sparql(), QSparqlQuery::InsertStatement);
}

/* --- CDTpSparqlQuery --- */

CDTpSparqlQuery::CDTpSparqlQuery(const CDTpQueryBuilder &builder, QObject *parent)
        : QObject(parent), mErrorSet(false)
{
    static uint counter = 0;
    mId = ++counter;
    mTime.start();

    debug() << "query" << mId << "started";
    debug() << builder.sparql(Options::DefaultSparqlOptions | Options::PrettyPrint);

    QSparqlConnection &connection = com::nokia::contactsd::SparqlConnectionManager::defaultConnection();
    QSparqlResult *result = connection.exec(builder.sparqlQuery());

    if (not result) {
        warning() << Q_FUNC_INFO << " - QSparqlConnection::exec() == 0";
        deleteLater();
        return;
    }
    if (result->hasError()) {
        warning() << Q_FUNC_INFO << result->lastError().message();
        delete result;
        deleteLater();
        return;
    }

    result->setParent(this);
    connect(result, SIGNAL(finished()), SLOT(onQueryFinished()), Qt::QueuedConnection);
}

void CDTpSparqlQuery::onQueryFinished()
{
    QSparqlResult *result = qobject_cast<QSparqlResult *>(sender());

    if (not result) {
        warning() << "QSparqlQuery finished with error:" << "Invalid signal sender";
        mErrorSet = true;
    } else {
        if (result->hasError()) {
            mErrorSet = true;
            mError = result->lastError();
            warning() << "QSparqlQuery finished with error:" << mError.message();
        }
        result->deleteLater();
    }

    debug() << "query" << mId << "finished. Time elapsed (ms):" << mTime.elapsed();

    Q_EMIT finished(this);

    deleteLater();
}

/* --- CDTpAccountsSparqlQuery --- */

CDTpAccountsSparqlQuery::CDTpAccountsSparqlQuery(const QList<CDTpAccountPtr> &accounts,
    const CDTpQueryBuilder &builder, QObject *parent)
        : CDTpSparqlQuery(builder, parent), mAccounts(accounts)
{
}

CDTpAccountsSparqlQuery::CDTpAccountsSparqlQuery(const CDTpAccountPtr &accountWrapper,
    const CDTpQueryBuilder &builder, QObject *parent)
        : CDTpSparqlQuery(builder, parent), mAccounts(QList<CDTpAccountPtr>() << accountWrapper)
{
}

