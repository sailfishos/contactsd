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
