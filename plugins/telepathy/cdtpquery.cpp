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

