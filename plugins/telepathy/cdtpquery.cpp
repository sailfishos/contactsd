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

const QString CDTpQueryBuilder::defaultGraph = QString::fromLatin1("<urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>");
static const QString indent = QString::fromLatin1("    ");
static const QString indent2 = indent + indent;

CDTpQueryBuilder::CDTpQueryBuilder(const char *text) : mVCount(0), mName(text)
{
}

void CDTpQueryBuilder::createResource(const QString &resource, const char *type, const QString &graph)
{
    mInsertPart[graph][resource] << QString::fromLatin1("a %1").arg(QLatin1String(type));
}

void CDTpQueryBuilder::insertProperty(const QString &resource, const char *property, const QString &value, const QString &graph)
{
    mInsertPart[graph][resource] << QString::fromLatin1("%1 %2").arg(QLatin1String(property)).arg(value);
}

void CDTpQueryBuilder::deleteResource(const QString &resource)
{
    append(mDeletePart, QString::fromLatin1("%1 a rdfs:Resource.").arg(resource));
}

void CDTpQueryBuilder::deleteProperty(const QString &resource, const char *property, const QString &value)
{
    append(mDeletePart, QString::fromLatin1("%1 %2 %3.").arg(resource).arg(QLatin1String(property)).arg(value));
}

QString CDTpQueryBuilder::deleteProperty(const QString &resource, const char *property)
{
    const QString value = uniquify();

    append(mDeletePart, QString::fromLatin1("%1 %2 %3.").arg(resource).arg(QLatin1String(property)).arg(value));
    append(mDeletePartWhere, QString::fromLatin1("OPTIONAL { %1 %2 %3 }.")
            .arg(resource).arg(QLatin1String(property)).arg(value));

    return value;
}

QString CDTpQueryBuilder::deletePropertyWithGraph(const QString &resource, const char *property, const QString &graph)
{
    const QString value = uniquify();

    append(mDeletePart, QString::fromLatin1("%1 %2 %3.").arg(resource).arg(QLatin1String(property)).arg(value));
    append(mDeletePartWhere, QString::fromLatin1("OPTIONAL { GRAPH %1 { %2 %3 %4 } }.")
            .arg(graph).arg(resource).arg(QLatin1String(property)).arg(value));

    return value;
}

QString CDTpQueryBuilder::deletePropertyAndLinkedResource(const QString &resource, const char *property)
{
    const QString oldValue = deleteProperty(resource, property);
    deleteResource(oldValue);
    return oldValue;
}

QString CDTpQueryBuilder::updateProperty(const QString &resource, const char *property, const QString &value, const QString &graph)
{
    const QString oldValue = deleteProperty(resource, property);
    insertProperty(resource, property, value, graph);
    return oldValue;
}

void CDTpQueryBuilder::appendRawSelection(const QString &str)
{
    append(mInsertPartWhere, str);
    append(mDeletePartWhere, str);
}

void CDTpQueryBuilder::appendRawQuery(const QString &str)
{
    mSubQueries << str;
}

void CDTpQueryBuilder::appendRawQuery(const CDTpQueryBuilder &builder)
{
    mSubQueries << builder.getRawQuery();
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

    // WHERE part
    QString insertWhereLines = setIndentation(mInsertPartWhere, indent);
    QString deleteWhereLines = setIndentation(mDeletePartWhere, indent);

    // Build final query
    QString rawQuery;
    rawQuery += QString::fromLatin1("# --- START %1 ---\n").arg(mName);
    if (!deleteLines.isEmpty()) {
        rawQuery += QString::fromLatin1("DELETE {\n%1\n}\n").arg(deleteLines);
        if (!deleteWhereLines.isEmpty()) {
            rawQuery += QString::fromLatin1("WHERE {\n%1\n}\n").arg(deleteWhereLines);
        }
    }
    if (!insertLines.isEmpty()) {
        rawQuery += QString::fromLatin1("INSERT {\n%1\n}\n").arg(insertLines);
        if (!insertWhereLines.isEmpty()) {
            rawQuery += QString::fromLatin1("WHERE {\n%1\n}\n").arg(insertWhereLines);
        }
    }

    // Append raw queries
    Q_FOREACH (const QString &subQuery, mSubQueries) {
        rawQuery += subQuery;
    }

    rawQuery += QString::fromLatin1("# --- END %1 ---\n").arg(mName);

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
            debug() << "QSparqlQuery finished with error:" << result->lastError().message();
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

