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
