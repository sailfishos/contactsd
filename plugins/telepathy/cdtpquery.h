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
    CDTpQueryBuilder(const char *text = "");

    static const Value defaultGraph;

    void createResource(const Value &resource, const char *type, const Value &graph = defaultGraph);
    void insertProperty(const Value &resource, const char *property, const Value &value, const Value &graph = defaultGraph);
    void deleteResource(const Value &resource);
    void deleteProperty(const Value &resource, const char *property, const Value &value);
    Value deleteProperty(const Value &resource, const char *property);
    Value deletePropertyWithGraph(const Value &resource, const char *property, const Value &graph);

    void appendRawSelection(const QString &str);
    void appendRawSelectionInsert(const QString &str);
    void appendRawSelectionDelete(const QString &str);

    void appendRawQuery(const QString &str);
    void appendRawQuery(const CDTpQueryBuilder &builder);
    void prependRawQuery(const QString &str);
    void prependRawQuery(const CDTpQueryBuilder &builder);

    QLatin1String name() const { return mName; };
    QString uniquify(const char *v = "v");
    QString getRawQuery() const;
    QSparqlQuery getSparqlQuery() const;

private:
    void append(QString &part, const QString &str);
    QString setIndentation(const QString &part, const QString &indentation) const;
    QString buildInsertPart(const QHash<QString, QStringList> &part) const;

    // graph <> (resource <> (list of "property value"))
    typedef QHash<QString, QHash<QString, QStringList> >InsertPart;
    InsertPart mInsertPart;
    QString mInsertPartWhere;
    QString mDeletePart;
    QString mDeletePartWhere;
    QList<QString> mPreQueries;
    QList<QString> mSubQueries;

    int mVCount;
    QLatin1String mName;
};

/* --- CDTpSparqlQuery --- */

class CDTpSparqlQuery : public QObject
{
    Q_OBJECT

public:
    CDTpSparqlQuery(const CDTpQueryBuilder &builder, QObject *parent = 0);
    ~CDTpSparqlQuery() {};

Q_SIGNALS:
    void finished(CDTpSparqlQuery *query);

private Q_SLOTS:
    void onQueryFinished();

private:
    uint mId;
    QTime mTime;
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
