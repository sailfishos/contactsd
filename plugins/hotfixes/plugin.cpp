/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "base-plugin.h"
#include "debug.h"

#include "qmheartbeat.h"

namespace Contactsd {

using MeeGo::QmHeartbeat;

class HotFixesPlugin : public BasePlugin
{
    Q_OBJECT

public:
    HotFixesPlugin()
        : m_heartbeat(0)
        , m_timer(0)
    {
    }

    ~HotFixesPlugin()
    {
        if (m_heartbeat) {
            m_heartbeat->close();
        }
    }

    void init()
    {
        if (m_heartbeat) {
            m_heartbeat->close();
            m_heartbeat->deleteLater();
            m_heartbeat = 0;
        }

        m_heartbeat = new QmHeartbeat(this);

        if (m_heartbeat->open(QmHeartbeat::SignalNeeded)) {
            connect(m_heartbeat, SIGNAL(wakeUp(QTime)), this, SLOT(onWakeUp()));
        } else {
            warning() << "hotfixes - Hearthbeat service not available, using fallback mechanism.";
            m_heartbeat->deleteLater();
            m_heartbeat = 0;

            m_timer = new QTimer(this);
            m_timer->setSingleShot(true);
            connect(m_timer, SIGNAL(timeout()), this, SLOT(onWakeUp()));
        }

        scheduleWakeUp(0);
    }

    QMap<QString, QVariant> metaData()
    {
        MetaData data;
        data[metaDataKeyName]    = QVariant(QString::fromLatin1("contacts-hotfixes"));
        data[metaDataKeyVersion] = QVariant(QString::fromLatin1("1"));
        data[metaDataKeyComment] = QVariant(QString::fromLatin1("Hotfixes for the contacts framework"));
        return data;
    }

private:
    void scheduleWakeUp(unsigned short wakeupSlot = QmHeartbeat::WAKEUP_SLOT_10_HOURS)
    {
        debug() << "hotfixes - scheduling wakeup within" << wakeupSlot << "seconds";

        if (m_heartbeat) {
            m_heartbeat->wait(wakeupSlot,
                              QmHeartbeat::WAKEUP_SLOT_10_HOURS,
                              QmHeartbeat::DoNotWaitHeartbeat);
        } else if (m_timer) {
            // seems "wakeup slots" are just a delay in seconds
            m_timer->setInterval(wakeupSlot * 1000);
            m_timer->start();
        }
    }

    bool runQuery(const QSparqlQuery &query, const char *slot)
    {
        QSparqlQueryOptions options;
        options.setPriority(QSparqlQueryOptions::LowPriority);
        QSparqlResult *const result = sparqlConnection().exec(query);

        if (result->hasError()) {
            warning() << "hotfixes - query failed:" << result->lastError().message();
            result->deleteLater();
            return false;
        }

        connect(result, SIGNAL(finished()), this, slot);
        return true;
    }

    bool runLookupQuery()
    {
        debug() << "hotfixes - running lookup query";

        m_garbageTuples.clear();

        static const QLatin1String queryString
                ("SELECT ?contact ?date {\n"
                 "  ?contact a nco:Contact\n"
                 "  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {\n"
                 "    ?contact dc:date ?date\n"
                 "  }\n"
                 "  FILTER(!EXISTS { ?contact nie:contentLastModified ?date }\n"
                 "      && !EXISTS { ?contact nie:contentAccessed ?date }\n"
                 "      && !EXISTS { ?contact nie:contentCreated ?date })\n"
                 "} LIMIT 1500");

        const QSparqlQuery query(queryString, QSparqlQuery::SelectStatement);
        return runQuery(query, SLOT(onLookupQueryFinished()));
    }

    bool runCleanupQuery()
    {
        if (m_garbageTuples.isEmpty()) {
            return false;
        }

        debug() << "hotfixes - running cleanup query";

        QStringList queryTokens;

        queryTokens += QLatin1String("DELETE {");

        for(int i = 0; i < 150 && not m_garbageTuples.isEmpty(); ++i) {
            const GarbageTuple tuple = m_garbageTuples.takeFirst();

            queryTokens += QString::fromLatin1
                    ("  <%1> nie:informationElementDate \"%2\" ; dc:date \"%2\" .").
                    arg(tuple.first, tuple.second);
        }

        queryTokens += QLatin1String("}");

        const QSparqlQuery query(queryTokens.join(QLatin1String("\n")),
                                 QSparqlQuery::DeleteStatement);
        return runQuery(query, SLOT(onCleanupQueryFinished()));
    }

private slots:
    void onWakeUp()
    {
        if (not runLookupQuery()) {
            // sleep if no lookup was possible
            scheduleWakeUp();
        }
    }

    void onLookupQueryFinished()
    {
        QSparqlResult *const result = qobject_cast<QSparqlResult *>(sender());

        while(result->next()) {
            m_garbageTuples += qMakePair(result->stringValue(0),
                                         result->stringValue(1));
        }

        result->deleteLater();

        if (not runCleanupQuery()) {
            // sleep if no cleanup was needed (or possible)
            scheduleWakeUp();
        }
    }

    void onCleanupQueryFinished()
    {
        sender()->deleteLater();

        if (not runCleanupQuery()) {
            // sleep if no further cleanup was needed (or possible)
            scheduleWakeUp(QmHeartbeat::WAKEUP_SLOT_30_SEC);
        }
    }

private:
    typedef QPair<QString, QString> GarbageTuple;

    QmHeartbeat *m_heartbeat;
    QTimer *m_timer;

    QSparqlQuery m_checkupQuery;
    QList<GarbageTuple> m_garbageTuples;
};

} // namespace Contactsd

Q_EXPORT_PLUGIN2(garbagecollectorplugin, Contactsd::HotFixesPlugin)

#include "plugin.moc"
