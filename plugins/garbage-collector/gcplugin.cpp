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

#include "gcplugin.h"
#include "garbagecollectoradaptor.h"

#include "debug.h"

// For fsync (impossible to do with Qt directly)
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

using namespace Contactsd;

const QLatin1String DBusObjectPath("/com/nokia/contacts/GarbageCollector1");

GcPlugin::GcPlugin()
{
    if (registerDBusObject()) {
        (void) new GarbageCollectorAdaptor(this);
    }

    loadSavedQueries();
}

GcPlugin::~GcPlugin()
{
    QDBusConnection::sessionBus().unregisterObject(DBusObjectPath);
}

void GcPlugin::init()
{
    debug() << "Initializing contactsd GarbageCollector plugin";
}

bool GcPlugin::registerDBusObject()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        warning() << "Could not connect to DBus:" << connection.lastError();
        return false;
    }

    if (!connection.registerObject(DBusObjectPath, this)) {
        warning() << "Could not register DBus object '/':" <<
            connection.lastError();
        return false;
    }
    return true;
}

void GcPlugin::loadSavedQueries()
{
    const QHash<QString, QueryStorage::Query> queries = mStorage.queries();

    for(QHash<QString, QueryStorage::Query>::const_iterator it = queries.constBegin();
        it != queries.constEnd(); ++it) {
        debug() << "Loading saved query" << it.key() << it.value().first<< it.value().second;
        Collector *c = new Collector(it.key(), it.value().first, this);
        c->setLoad(it.value().second);
        mCollectors[it.key()] = c;
    }
}

void GcPlugin::Register(const QString &id, const QString &query)
{
    mStorage.addQuery(id, query);

    if (mCollectors.contains(id)) {
        mCollectors[id]->setQuery(query);
        return;
    }

    mCollectors[id] = new Collector(id, query, this);
    debug() << "New GarbageCollector query registered for id" << id;
}

void GcPlugin::Unregister(const QString &id)
{
    if (!mCollectors.contains(id)) {
        debug() << "Error: unknown collector id" << id;
        return;
    }

    debug() << "Unregistering query with id" << id;

    mStorage.removeQuery(id);
    delete mCollectors.take(id);
}

void GcPlugin::Trigger(const QString &id, double load_increment)
{
    if (!mCollectors.contains(id)) {
        debug() << "Error: unknown collector id" << id;
        return;
    }

    Collector *c = mCollectors[id];
    c->trigger(load_increment);

    debug() << "increased query" << id << "by" << load_increment << "total load:" << c->load();

    mStorage.updateLoad(id, c->load());
}

GcPlugin::MetaData GcPlugin::metaData()
{
    MetaData data;
    data[metaDataKeyName]    = QVariant(QString::fromLatin1("garbage-collector"));
    data[metaDataKeyVersion] = QVariant(QString::fromLatin1("0.1"));
    data[metaDataKeyComment] = QVariant(QString::fromLatin1("contactsd garbage collector plugin"));
    return data;
}

///////////////////////////////////////////////////////////////////////////////

static const int TriggerTimeout = 10; // seconds

Collector::Collector(const QString &id, const QString &q, QObject *parent)
    : QObject(parent)
    , mId(id)
    , mQuery(q)
    , mLoad(0)
{
    mTimer.setInterval(TriggerTimeout*1000);
    mTimer.setSingleShot(true);
    connect(&mTimer, SIGNAL(timeout()), SLOT(onTimeout()));
}

void Collector::trigger(double v)
{
    mLoad += v;
    if (mLoad >= 1) {
        // Restarts the timer if already running
        mTimer.start();
    }
}

void Collector::onTimeout()
{
    debug() << "Launch query for collector" << mId;
    mLoad = 0;

    QSparqlConnection &connection = BasePlugin::sparqlConnection();
    static QSparqlQueryOptions queryOptions;
    queryOptions.setPriority(QSparqlQueryOptions::LowPriority);

    QSparqlResult *result = connection.exec(QSparqlQuery(mQuery, QSparqlQuery::DeleteStatement),
                                            queryOptions);

    if (not result) {
        warning() << "QSparqlConnection::exec() == 0";
        return;
    }
    if (result->hasError()) {
        warning() << "Error exec query:" << result->lastError().message();
        delete result;
        return;
    }

    result->setParent(this);
    connect(result, SIGNAL(finished()), SLOT(onQueryFinished()), Qt::QueuedConnection);
}

void Collector::onQueryFinished()
{
    QSparqlResult *result = qobject_cast<QSparqlResult *>(sender());

    if (not result) {
        warning() << "QSparqlQuery finished with error:" << "Invalid signal sender";
    } else {
        if (result->hasError()) {
            warning() << "QSparqlQuery finished with error:" << result->lastError();
        }
        result->deleteLater();
    }
}

///////////////////////////////////////////////////////////////////////////////

QueryStorage::QueryStorage()
{
    // We can't use a simple QSettings, because it's not crash-safe (ie can get
    // corrupted if device crashes while saving). So we just use two simple
    // custom function (un)serializing the QHash to a file.
    load();
}

QueryStorage::~QueryStorage()
{
}

QHash<QString, QueryStorage::Query>
QueryStorage::queries() const
{
    return mQueries;
}

void QueryStorage::setQueries(const QHash<QString, Query> &queries)
{
    mQueries = queries;
    save();
}

void QueryStorage::addQuery(const QString &id, const QString &query)
{
    Query q = qMakePair(query, 0.);
    mQueries.insert(id, q);
    save();
}

void QueryStorage::updateLoad(const QString &id, double value)
{
    QHash<QString, Query>::iterator it = mQueries.find(id);

    if (it == mQueries.constEnd()) {
        return;
    }

    Query q = *it;
    q.second = value;

    mQueries.insert(id, q);

    save();
}

void QueryStorage::removeQuery(const QString &id)
{
    if (mQueries.remove(id)) {
        save();
    }
}

QString QueryStorage::filePath()
{
    static const QString cacheFileName = "queries";
    static QString path;

    if (not path.isNull()) {
        return path;
    }

    QDir cacheDir;
    const char *xdgCacheDir = getenv("XDG_CACHE_DIR");

    if (xdgCacheDir != 0) {
        cacheDir = QDir(QString::fromLocal8Bit(xdgCacheDir).append("/contactsd"));
    } else {
        cacheDir = QDir(QDir::home().absoluteFilePath(QString::fromLatin1(".cache/contactsd")));
    }

    if (not cacheDir.exists()) {
        if (not QDir::root().mkpath(cacheDir.absolutePath())) {
            warning() << "Could not create cache dir";
            return QString();
        }
    }

    path = cacheDir.absoluteFilePath(cacheFileName);
    return path;
}

void QueryStorage::load()
{
    QFile file(filePath());

    if (not file.exists()) {
        return;
    }

    if (not file.open(QIODevice::ReadOnly)) {
        warning() << "Could not open queries file " << file.fileName() << ":" << file.errorString();
        return;
    }

    QDataStream stream(&file);
    stream >> mQueries;

    file.close();
}

void QueryStorage::save()
{
    QTemporaryFile tmpFile(filePath());
    tmpFile.setAutoRemove(false);

    if (not tmpFile.open()) {
        warning() << "Could not create temporary file:" << tmpFile.errorString();
        return;
    }

    QDataStream stream(&tmpFile);
    stream << mQueries;

    if (not tmpFile.flush()) {
        warning() << "Could not write queries to disk:" << tmpFile.errorString();
        return;
    }

    int errorCode = -1;

    if ((errorCode = fsync(tmpFile.handle())) != 0) {
        warning() << "Could not sync queries to disk:" << strerror(errorCode);
        return;
    }

    tmpFile.close();

    // We can't use Qt's rename because it won't overwrite an existing file
    errorCode = -1;
    if (rename(tmpFile.fileName().toLocal8Bit(), filePath().toLocal8Bit()) != 0) {
        warning() << "Could not overwrite old queries file:" << strerror(errorCode);
        return;
    }
}

///////////////////////////////////////////////////////////////////////////////

Q_EXPORT_PLUGIN2(garbagecollectorplugin, GcPlugin)
