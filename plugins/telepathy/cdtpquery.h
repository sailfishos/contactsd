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
    CDTpQueryBuilder();

    void append(const UpdateBase &q);
    void prepend(const UpdateBase &q);

    void append(const CDTpQueryBuilder &builder);
    void prepend(const CDTpQueryBuilder &builder);

    QString sparql(Options::SparqlOptions options = Options::DefaultSparqlOptions) const;
    QSparqlQuery sparqlQuery() const;

private:
    QList<UpdateBase> mQueries;
};

/* --- CDTpSparqlQuery --- */

class CDTpSparqlQuery : public QObject
{
    Q_OBJECT

public:
    CDTpSparqlQuery(const CDTpQueryBuilder &builder, QObject *parent = 0);
    ~CDTpSparqlQuery() {}

    bool hasError() { return mErrorSet; }
    QSparqlError error() { return mError; }

Q_SIGNALS:
    void finished(CDTpSparqlQuery *query);

private Q_SLOTS:
    void onQueryFinished();

private:
    uint mId;
    QTime mTime;
    bool mErrorSet;
    QSparqlError mError;
    QSparqlResult *mResult;
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
