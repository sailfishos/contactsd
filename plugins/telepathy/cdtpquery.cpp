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

const Value CDTpQueryBuilder::defaultGraph = ResourceValue(QString::fromLatin1("urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9"));
static const QString indent = QString::fromLatin1("    ");
static const QString indent2 = indent + indent;

CDTpQueryBuilder::CDTpQueryBuilder(const char *text) : mVCount(0), mName(text)
{
}

void CDTpQueryBuilder::createResource(const Value &resource, const char *type, const Value &graph)
{
    mInsertPart[graph.sparql()][resource.sparql()] << QString::fromLatin1("a %1").arg(QLatin1String(type));
}

void CDTpQueryBuilder::insertProperty(const Value &resource, const char *property, const Value &value, const Value &graph)
{
    mInsertPart[graph.sparql()][resource.sparql()] << QString::fromLatin1("%1 %2").arg(QLatin1String(property)).arg(value.sparql());
}

void CDTpQueryBuilder::deleteResource(const Value &resource)
{
    append(mDeletePart, QString::fromLatin1("%1 a rdfs:Resource.").arg(resource.sparql()));
}

void CDTpQueryBuilder::deleteProperty(const Value &resource, const char *property, const Value &value)
{
    append(mDeletePart, QString::fromLatin1("%1 %2 %3.").arg(resource.sparql()).arg(QLatin1String(property)).arg(value.sparql()));
}

Value CDTpQueryBuilder::deleteProperty(const Value &resource, const char *property)
{
    const Value v = Variable(uniquify());

    append(mDeletePart, QString::fromLatin1("%1 %2 %3.").arg(resource.sparql()).arg(QLatin1String(property)).arg(v.sparql()));
    append(mDeletePartWhere, QString::fromLatin1("OPTIONAL { %1 %2 %3 }.")
            .arg(resource.sparql()).arg(QLatin1String(property)).arg(v.sparql()));

    return v;
}

Value CDTpQueryBuilder::deletePropertyWithGraph(const Value &resource, const char *property, const Value &graph)
{
    const Value v = Variable(uniquify());

    append(mDeletePart, QString::fromLatin1("%1 %2 %3.").arg(resource.sparql()).arg(QLatin1String(property)).arg(v.sparql()));
    append(mDeletePartWhere, QString::fromLatin1("OPTIONAL { GRAPH %1 { %2 %3 %4 } }.")
            .arg(graph.sparql()).arg(resource.sparql()).arg(QLatin1String(property)).arg(v.sparql()));

    return v;
}

void CDTpQueryBuilder::appendRawSelection(const QString &str)
{
    appendRawSelectionInsert(str);
    appendRawSelectionDelete(str);
}

void CDTpQueryBuilder::appendRawSelectionInsert(const QString &str)
{
    append(mInsertPartWhere, str);
}

void CDTpQueryBuilder::appendRawSelectionDelete(const QString &str)
{
    append(mDeletePartWhere, str);
}

void CDTpQueryBuilder::appendRawQuery(const QString &str)
{
    if (!str.isEmpty()) {
        mSubQueries << str;
    }
}

void CDTpQueryBuilder::appendRawQuery(const CDTpQueryBuilder &builder)
{
    appendRawQuery(builder.getRawQuery());
}

void CDTpQueryBuilder::prependRawQuery(const QString &str)
{
    if (!str.isEmpty()) {
        mPreQueries << str;
    }
}

void CDTpQueryBuilder::prependRawQuery(const CDTpQueryBuilder &builder)
{
    prependRawQuery(builder.getRawQuery());
}


QString CDTpQueryBuilder::uniquify(const char *v)
{
    return QString::fromLatin1("%1_%2").arg(QLatin1String(v)).arg(++mVCount);
}

QString CDTpQueryBuilder::getRawQuery() const
{
    // DELETE part
    QString deleteLines = setIndentation(mDeletePart, indent);

    // INSERT part
    QString insertLines;
    InsertPart::const_iterator i;
    for (i = mInsertPart.constBegin(); i != mInsertPart.constEnd(); ++i) {
        QString graphLines = setIndentation(buildInsertPart(i.value()), indent2);
        if (!graphLines.isEmpty()) {
            insertLines += indent + QString::fromLatin1("GRAPH %1 {\n").arg(i.key());
            insertLines += graphLines + QString::fromLatin1("\n");
            insertLines += indent + QString::fromLatin1("}\n");
        }
    }

    // Build final query
    bool empty = true;
    QString rawQuery;
    rawQuery += QString::fromLatin1("# --- START %1 ---\n").arg(mName);

    // Prepend raw queries
    Q_FOREACH (const QString &subQuery, mPreQueries) {
        empty = false;
        rawQuery += subQuery;
    }

    if (!deleteLines.isEmpty()) {
        empty = false;
        rawQuery += QString::fromLatin1("DELETE {\n%1\n}\n").arg(deleteLines);
        QString deleteWhereLines = setIndentation(mDeletePartWhere, indent);
        if (!deleteWhereLines.isEmpty()) {
            rawQuery += QString::fromLatin1("WHERE {\n%1\n}\n").arg(deleteWhereLines);
        }
    }
    if (!insertLines.isEmpty()) {
        empty = false;
        rawQuery += QString::fromLatin1("INSERT OR REPLACE {\n%1\n}\n").arg(insertLines);
        QString insertWhereLines = setIndentation(mInsertPartWhere, indent);
        if (!insertWhereLines.isEmpty()) {
            rawQuery += QString::fromLatin1("WHERE {\n%1\n}\n").arg(insertWhereLines);
        }
    }

    // Append raw queries
    Q_FOREACH (const QString &subQuery, mSubQueries) {
        empty = false;
        rawQuery += subQuery;
    }

    rawQuery += QString::fromLatin1("# --- END %1 ---\n").arg(mName);

    if (empty) {
         return QString();
    }

    return rawQuery;
}

void CDTpQueryBuilder::append(QString &part, const QString &str)
{
    if (!part.isEmpty()) {
        part += QString::fromLatin1("\n");
    }
    part += str;
}

QString CDTpQueryBuilder::setIndentation(const QString &part, const QString &indentation) const
{
    if (part.isEmpty()) {
        return QString();
    }
    return indentation + QString(part).replace(QChar::fromLatin1('\n'),
            QString::fromLatin1("\n") + indentation);
}

QString CDTpQueryBuilder::buildInsertPart(const QHash<QString, QStringList> &part) const
{
    QString ret;

    QHash<QString, QStringList>::const_iterator i;
    for (i = part.constBegin(); i != part.constEnd(); ++i) {
        if (!ret.isEmpty()) {
            ret += QString::fromLatin1("\n");
        }
        ret += QString::fromLatin1("%1 %2.").arg(i.key())
                .arg(i.value().join(QString::fromLatin1(";\n    ")));
    }

    return ret;
}

QSparqlQuery CDTpQueryBuilder::getSparqlQuery() const
{
    return QSparqlQuery(getRawQuery(), QSparqlQuery::InsertStatement);
}

/* --- CDTpSparqlQuery --- */

CDTpSparqlQuery::CDTpSparqlQuery(const CDTpQueryBuilder &builder, QObject *parent)
        : QObject(parent)
{
    static uint counter = 0;
    mId = ++counter;
    mTime.start();

    debug() << "query" << mId << "started:" << builder.name();
    debug() << builder.getRawQuery();

    QSparqlConnection &connection = com::nokia::contactsd::SparqlConnectionManager::defaultConnection();
    QSparqlResult *result = connection.exec(builder.getSparqlQuery());

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
    QSparqlResult *const result = qobject_cast<QSparqlResult *>(sender());

    if (not result) {
        warning() << "QSparqlQuery finished with error:" << "Invalid signal sender";
    } else {
        if (result->hasError()) {
            warning() << "QSparqlQuery finished with error:" << result->lastError().message();
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

