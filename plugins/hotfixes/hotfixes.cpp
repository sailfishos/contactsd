/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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
 *********************************************************************************/


#include "hotfixes.h"
#include "debug.h"

#include <QtCore>
#include <qmheartbeat.h>

namespace Contactsd {

using MeeGo::QmHeartbeat;

typedef QPair<QString, QString> GarbageTuple;

class HotFixesPrivate
{
    HotFixesPrivate(QSparqlConnection &connection)
        : connection(connection)
        , heartbeat(0)
        , timer(0)
    {
    }

    QSparqlConnection &connection;
    QmHeartbeat *heartbeat;
    QTimer *timer;

    QSparqlQuery checkupQuery;
    QList<GarbageTuple> garbageTuples;

    friend class HotFixes;
};

HotFixes::HotFixes(QSparqlConnection &connection, QObject *parent)
    : QObject(parent)
    , d(new HotFixesPrivate(connection))
{
    if (d->heartbeat) {
        d->heartbeat->close();
        d->heartbeat->deleteLater();
        d->heartbeat = 0;
    }

    d->heartbeat = new QmHeartbeat(this);

    if (d->heartbeat->open(QmHeartbeat::SignalNeeded)) {
        connect(d->heartbeat, SIGNAL(wakeUp(QTime)), this, SLOT(onWakeUp()));
    } else {
        warning() << "hotfixes - Hearthbeat service not available, using fallback mechanism.";
        d->heartbeat->deleteLater();
        d->heartbeat = 0;

        d->timer = new QTimer(this);
        d->timer->setSingleShot(true);
        connect(d->timer, SIGNAL(timeout()), this, SLOT(onWakeUp()));
    }

    scheduleWakeUp(0, QmHeartbeat::WAKEUP_SLOT_2_5_MINS);
}

HotFixes::~HotFixes()
{
    if (d->heartbeat) {
        d->heartbeat->close();
    }


    delete d;
}

void HotFixes::scheduleWakeUp()
{
    scheduleWakeUp(MeeGo::QmHeartbeat::WAKEUP_SLOT_10_HOURS,
                   MeeGo::QmHeartbeat::WAKEUP_SLOT_10_HOURS);
}

void HotFixes::scheduleWakeUp(ushort minimumDelay, ushort maximumDelay)
{
    debug() << "hotfixes - scheduling wakeup within" << minimumDelay << "seconds";

    if (d->heartbeat) {
        d->heartbeat->wait(minimumDelay, maximumDelay,
                          QmHeartbeat::DoNotWaitHeartbeat);
    } else if (d->timer) {
        // seems "wakeup slots" are just a delay in seconds
        d->timer->setInterval((minimumDelay + maximumDelay) * 1000 / 2);
        d->timer->start();
    }
}

bool HotFixes::runQuery(const QSparqlQuery &query, const char *slot)
{
    QSparqlQueryOptions options;
    options.setPriority(QSparqlQueryOptions::LowPriority);
    QSparqlResult *const result = d->connection.exec(query);

    if (result->hasError()) {
        warning() << "hotfixes - query failed:" << result->lastError().message();
        result->deleteLater();
        return false;
    }

    connect(result, SIGNAL(finished()), this, slot);
    return true;
}

bool HotFixes::runLookupQuery()
{
    debug() << "hotfixes - running lookup query";

    d->garbageTuples.clear();

    static const QLatin1String queryString
            ("SELECT ?contact ?date {\n"
             "  ?contact a nco:Contact\n"
             "  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {\n"
             "    ?contact dc:date ?date\n"
             "  }\n"
             "  FILTER(!EXISTS { ?contact nie:contentLastModified ?date }\n"
             "      && !EXISTS { ?contact nie:contentAccessed ?date }\n"
             "      && !EXISTS { ?contact nie:contentCreated ?date })\n"
             "} LIMIT 200");

    const QSparqlQuery query(queryString, QSparqlQuery::SelectStatement);
    return runQuery(query, SLOT(onLookupQueryFinished()));
}

bool HotFixes::runCleanupQuery()
{
    if (d->garbageTuples.isEmpty()) {
        return false;
    }

    debug() << "hotfixes - running cleanup query";

    QStringList queryTokens;

    queryTokens += QLatin1String("DELETE {");

    for(int i = 0; i < 40 && not d->garbageTuples.isEmpty(); ++i) {
        const GarbageTuple tuple = d->garbageTuples.takeFirst();

        queryTokens += QString::fromLatin1
                ("  <%1> nie:informationElementDate \"%2\" ; dc:date \"%2\" .").
                arg(tuple.first, tuple.second);
    }

    queryTokens += QLatin1String("}");

    const QSparqlQuery query(queryTokens.join(QLatin1String("\n")),
                             QSparqlQuery::DeleteStatement);
    return runQuery(query, SLOT(onCleanupQueryFinished()));
}

void HotFixes::onWakeUp()
{
    if (not runLookupQuery()) {
        // sleep long if no lookup was possible
        scheduleWakeUp();
    }
}

void HotFixes::onLookupQueryFinished()
{
    QSparqlResult *const result = qobject_cast<QSparqlResult *>(sender());

    while(result->next()) {
        d->garbageTuples += qMakePair(result->stringValue(0),
                                     result->stringValue(1));
    }

    result->deleteLater();

    if (not runCleanupQuery()) {
        // sleep long if no cleanup was needed (or possible)
        scheduleWakeUp();
    }
}

void HotFixes::onCleanupQueryFinished()
{
    sender()->deleteLater();

    if (not runCleanupQuery()) {
        // sleep short if current batch of tuples has been processed
        scheduleWakeUp(QmHeartbeat::WAKEUP_SLOT_30_SEC,
                       QmHeartbeat::WAKEUP_SLOT_2_5_MINS);
    }
}


} // namespace Contactsd
