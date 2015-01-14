/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2015 Jolla Ltd.
 **
 ** Contact:  Matt Vogt (matthew.vogt@jollamobile.com)
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

#ifndef CDTPDEVICEPRESENCE_H
#define CDTPDEVICEPRESENCE_H

#include <QObject>

class CDTpDevicePresence : public QObject
{
    Q_OBJECT

public:
    CDTpDevicePresence(QObject *parent = 0);
    ~CDTpDevicePresence();

signals:
    void requestUpdate();

    void accountList(const QStringList &accountPaths);
    void globalUpdate(int presenceState);
    void update(const QString &accountPath,
                const QString &accountUri,
                const QString &serviceProvider,
                const QString &serviceProviderDisplayName,
                const QString &accountDisplayName,
                const QString &accountIconPath,
                int presenceState,
                const QString &presenceMessage,
                bool enabled);
};

#endif // CDTPDEVICEPRESENCE_H
