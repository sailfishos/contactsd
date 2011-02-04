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
#include "sparqlconnectionmanager.h"

/* --- CDTpQueryBuilder --- */

const QString CDTpQueryBuilder::defaultGraph = QLatin1String("<urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>");
const QString CDTpQueryBuilder::privateGraph = QLatin1String("<urn:uuid:679293d4-60f0-49c7-8d63-f1528fe31f66>");
const QString CDTpQueryBuilder::indent = QLatin1String("    ");
const QString CDTpQueryBuilder::indent2 = indent + indent;

CDTpQueryBuilder::CDTpQueryBuilder() : vCount(0)
{
}

void CDTpQueryBuilder::createResource(const QString &resource, const QString &type, const QString &graph)
{
    append(insertPart[graph], QString(QLatin1String("%1 a %2.")).arg(resource).arg(type));
}

void CDTpQueryBuilder::insertProperty(const QString &resource, const QString &property, const QString &value, const QString &graph)
{
    append(insertPart[graph], QString(QLatin1String("%1 %2 %3.")).arg(resource).arg(property).arg(value));
}

void CDTpQueryBuilder::deleteResource(const QString &resource)
{
    append(deletePart, QString(QLatin1String("%1 a rdfs:Resource.")).arg(resource));
}

void CDTpQueryBuilder::deleteProperty(const QString &resource, const QString &property, const QString &value)
{
    append(deletePart, QString(QLatin1String("%1 %2 %3.")).arg(resource).arg(property).arg(value));
}

QString CDTpQueryBuilder::deleteProperty(const QString &resource, const QString &property)
{
    const QString value = uniquify();

    append(deletePart, QString(QLatin1String("%1 %2 %3.")).arg(resource).arg(property).arg(value));
    append(deletePartWhere, QString(QLatin1String("OPTIONAL { %1 %2 %3 }."))
            .arg(resource).arg(property).arg(value));

    return value;
}

QString CDTpQueryBuilder::deletePropertyWithGraph(const QString &resource, const QString &property, const QString &graph)
{
    const QString value = uniquify();

    append(deletePart, QString(QLatin1String("%1 %2 %3.")).arg(resource).arg(property).arg(value));
    append(deletePartWhere, QString(QLatin1String("OPTIONAL { GRAPH %1 { %2 %3 %4 } }."))
            .arg(graph).arg(resource).arg(property).arg(value));

    return value;
}


QString CDTpQueryBuilder::deletePropertyAndLinkedResource(const QString &resource, const QString &property)
{
    const QString oldValue = deleteProperty(resource, property);
    deleteResource(oldValue);
    return oldValue;
}

QString CDTpQueryBuilder::updateProperty(const QString &resource, const QString &property, const QString &value, const QString &graph)
{
    const QString oldValue = deleteProperty(resource, property);
    insertProperty(resource, property, value, graph);
    return oldValue;
}

void CDTpQueryBuilder::appendRawSelection(const QString &str)
{
    append(insertPartWhere, str);
    append(deletePartWhere, str);
}

void CDTpQueryBuilder::appendSubBuilder(CDTpQueryBuilder *builder)
{
    subBuilders << builder;
}

QString CDTpQueryBuilder::uniquify(const QString &v)
{
    return QString(QLatin1String("%1_%2")).arg(v).arg(++vCount);
}

void CDTpQueryBuilder::mergeWithOptional(CDTpQueryBuilder &builder)
{
    // Append insertPart
    QHash<QString, QString>::const_iterator i;
    for (i = builder.insertPart.constBegin(); i != builder.insertPart.constEnd(); ++i) {
        append(insertPart[i.key()], i.value());
    }

    // Append deletePart
    append(deletePart, builder.deletePart);

    // Append Where part
    static const QString optionalTemplate = QString(QLatin1String("OPTIONAL {\n%1\n}."));
    append(insertPartWhere, optionalTemplate.arg(setIndentation(builder.insertPartWhere, indent)));
    append(deletePartWhere, optionalTemplate.arg(setIndentation(builder.deletePartWhere, indent)));
}

QString CDTpQueryBuilder::getRawQuery() const
{
    // DELETE part
    QString deleteLines = setIndentation(deletePart, indent);

    // INSERT part
    QString insertLines;
    QHash<QString, QString>::const_iterator i;
    for (i = insertPart.constBegin(); i != insertPart.constEnd(); ++i) {
        QString graphLines = setIndentation(i.value(), indent2);
        if (!graphLines.isEmpty()) {
            insertLines += indent + QString(QLatin1String("GRAPH %1 {\n")).arg(i.key());
            insertLines += graphLines + QLatin1String("\n");
            insertLines += indent + QLatin1String("}\n");
        }
    }

    // WHERE part
    QString insertWhereLines = setIndentation(insertPartWhere, indent);
    QString deleteWhereLines = setIndentation(deletePartWhere, indent);

    // Build final query
    QString rawQuery;
    if (!deleteLines.isEmpty()) {
        rawQuery += QString(QLatin1String("DELETE {\n%1\n}\n")).arg(deleteLines);
        if (!deleteWhereLines.isEmpty()) {
            rawQuery += QString(QLatin1String("WHERE {\n%1\n}\n")).arg(deleteWhereLines);
        }
    }
    if (!insertLines.isEmpty()) {
        rawQuery += QString(QLatin1String("INSERT {\n%1\n}\n")).arg(insertLines);
        if (!insertWhereLines.isEmpty()) {
            rawQuery += QString(QLatin1String("WHERE {\n%1\n}\n")).arg(insertWhereLines);
        }
    }

    // Recurse to sub builders
    Q_FOREACH (CDTpQueryBuilder *subBuilder, subBuilders) {
        rawQuery += subBuilder->getRawQuery();
    }

    return rawQuery;
}

void CDTpQueryBuilder::append(QString &part, const QString &str)
{
    if (!part.isEmpty()) {
        part += QLatin1String("\n");
    }
    part += str;
}

QString CDTpQueryBuilder::setIndentation(const QString &part, const QString &indentation) const
{
    if (part.isEmpty()) {
        return QString();
    }
    return indentation + QString(part).replace('\n', QLatin1String("\n") + indentation);
}

QSparqlQuery CDTpQueryBuilder::getSparqlQuery() const
{
    return QSparqlQuery(getRawQuery(), QSparqlQuery::InsertStatement);
}

void CDTpQueryBuilder::clear()
{
    insertPart.clear();
    insertPartWhere.clear();
    deletePart.clear();
    deletePartWhere.clear();
    vCount = 0;

    Q_FOREACH (CDTpQueryBuilder *subBuilder, subBuilders) {
        delete subBuilder;
    }
}

/* --- CDTpSparqlQuery --- */

CDTpSparqlQuery::CDTpSparqlQuery(QSparqlQuery sparqlQuery, QObject *parent)
        : QObject(parent)
{
    QSparqlConnection &connection = com::nokia::contactsd::SparqlConnectionManager::defaultConnection();
    QSparqlResult *result = connection.exec(sparqlQuery);

    if (not result) {
        qWarning() << Q_FUNC_INFO << " - QSparqlConnection::exec() == 0";
        deleteLater();
        return;
    }
    if (result->hasError()) {
        qWarning() << Q_FUNC_INFO << result->lastError().message();
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
        qWarning() << "QSparqlQuery finished with error:" << "Invalid signal sender";
    } else {
        if (result->hasError()) {
            qDebug() << "QSparqlQuery finished with error:" << result->lastError().message();
        }
        result->deleteLater();
    }

    Q_EMIT finished(this);

    deleteLater();
}

/* --- CDTpAccountsSparqlQuery --- */

CDTpAccountsSparqlQuery::CDTpAccountsSparqlQuery(const QList<CDTpAccountPtr> &accounts,
    QSparqlQuery sparqlQuery, QObject *parent)
        : CDTpSparqlQuery(sparqlQuery, parent), mAccounts(accounts)
{
}

