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


#ifndef CONTACTSD_HOTFIXES_H
#define CONTACTSD_HOTFIXES_H

#include <QtSparql>

namespace Contactsd {

class HotFixesPrivate;
class HotFixes : public QObject
{
    Q_OBJECT

public:
    explicit HotFixes(QSparqlConnection &connection, QObject *parent = 0);
    virtual ~HotFixes();

public slots:
    void onWakeUp();

private slots:
    void onLookupQueryFinished();
    void onCleanupQueryFinished();

private:
    void scheduleWakeUp();
    void scheduleWakeUp(ushort minimumDelay, ushort maximumDelay);
    bool runQuery(const QSparqlQuery &query, const char *slot);
    bool runLookupQuery();
    bool runCleanupQuery();

private:
    HotFixesPrivate *const d;
};

} // namespace Contactsd

#endif // CONTACTSD_HOTFIXES_H
