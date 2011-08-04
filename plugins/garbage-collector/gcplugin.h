/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#ifndef GCPLUGIN_H
#define GCPLUGIN_H

#include "base-plugin.h"

class Collector;
class QueryStorage;

class QueryStorage
{
public:
    // Query, Load
    typedef QPair<QString, double> Query;

    QueryStorage();
    ~QueryStorage();

    QHash<QString, Query> queries() const;
    void setQueries(const QHash<QString, Query> &queries);
    void addQuery(const QString &id, const QString &query);
    void updateLoad(const QString &id, double value);

private:
    static QString filePath();
    void load();
    void save();

    QHash<QString, Query> mQueries;
};

class GcPlugin : public Contactsd::BasePlugin
{
    Q_OBJECT

public:
    GcPlugin();
    ~GcPlugin();

    void init();
    MetaData metaData();

public Q_SLOTS:
    void Register(const QString &id, const QString &query);
    void Trigger(const QString &id, double load_increment);

private:
    bool registerDBusObject();
    void loadSavedQueries();

private:
    QueryStorage mStorage;
    QHash<QString, Collector*> mCollectors;
};

class Collector : public QObject
{
    Q_OBJECT

public:
    Collector(const QString &id, const QString &q, QObject *parent);

    void trigger(double v);
    void setLoad(double load) { mLoad = load; }
    double load() const { return mLoad; }
    void setQuery(const QString &query) { mQuery = query; }
    const QString &query() const { return mQuery; }

private Q_SLOTS:
    void onTimeout();

private:
    QSparqlQueryOptions mQueryOptions;
    const QString mId;
    QString mQuery;
    double mLoad;
    QTimer mTimer;
};

class CollectorResult : public QObject
{
    Q_OBJECT

public:
    CollectorResult(const QString &id, QSparqlResult *result, QObject *parent);

private Q_SLOTS:
    void onQueryFinished();

private:
    const QString mId;
    QSparqlResult *mResult;
};

#endif // GCPLUGIN_H
