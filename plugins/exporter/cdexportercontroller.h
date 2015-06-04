/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2014 Jolla Ltd.
 **
 ** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **/

#ifndef CDEXPORTERCONTROLLER_H
#define CDEXPORTERCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QSet>
#include <QStringList>
#include <QString>

#include <QContactManager>

#include <MGConfItem>

QTCONTACTS_USE_NAMESPACE

class CDExporterController : public QObject
{
    Q_OBJECT

public:
    explicit CDExporterController(QObject *parent = 0);
    ~CDExporterController();

private slots:
    void onPrivilegedContactsAdded(const QList<QContactId> &addedIds);
    void onPrivilegedContactsChanged(const QList<QContactId> &changedIds);
    void onPrivilegedContactsPresenceChanged(const QList<QContactId> &changedIds);
    void onPrivilegedContactsRemoved(const QList<QContactId> &removedIds);

    void onNonprivilegedContactsAdded(const QList<QContactId> &addedIds);
    void onNonprivilegedContactsChanged(const QList<QContactId> &changedIds);
    void onNonprivilegedContactsRemoved(const QList<QContactId> &removedIds);

    void onSyncContactsChanged(const QStringList &syncTargets);

    void onSyncTimeout();

private:
    enum ChangeType { PresenceChange, DataChange };
    void scheduleSync(ChangeType type);

    QContactManager m_privilegedManager;
    QContactManager m_nonprivilegedManager;
    QTimer m_syncTimer;
    QSet<QString> m_syncTargetsNeedingSync;

    MGConfItem m_disabledConf;
    MGConfItem m_debugConf;
    MGConfItem m_importConf;

    uint m_lastSyncTimestamp;
    int m_adaptiveSyncDelay;

    bool m_syncAllTargets;
};

#endif // CDEXPORTERCONTROLLER_H
