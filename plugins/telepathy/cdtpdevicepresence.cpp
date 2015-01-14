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
#include <QDBusConnection>

#include "cdtpdevicepresence.h"
#include "devicepresenceadaptor.h"


namespace {

const QString DEVICE_PRESENCE_OBJECT_PATH = QStringLiteral("/org/nemomobile/DevicePresence");
const QString DEVICE_PRESENCE_SERVICE_NAME = QStringLiteral("org.nemomobile.DevicePresence");

}

CDTpDevicePresence::CDTpDevicePresence(QObject *parent)
    : QObject(parent)
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        qCritical() << Q_FUNC_INFO << "ERROR: No DBus session bus found!";
        return;
    }

    if (!QDBusConnection::sessionBus().registerObject(DEVICE_PRESENCE_OBJECT_PATH, this)) {
        qWarning() << Q_FUNC_INFO
                   << "Object registration failed:" << DEVICE_PRESENCE_OBJECT_PATH
                   << QDBusConnection::sessionBus().lastError();
    } else {
        if (!QDBusConnection::sessionBus().registerService(DEVICE_PRESENCE_SERVICE_NAME)) {
            qWarning() << Q_FUNC_INFO
                       << "Unable to register account presence service:" << DEVICE_PRESENCE_SERVICE_NAME
                       << QDBusConnection::sessionBus().lastError();
        } else {
            new DevicePresenceAdaptor(this);
        }
    }
}

CDTpDevicePresence::~CDTpDevicePresence()
{
    QDBusConnection::sessionBus().unregisterService(DEVICE_PRESENCE_SERVICE_NAME);
    QDBusConnection::sessionBus().unregisterObject(DEVICE_PRESENCE_OBJECT_PATH);
}
