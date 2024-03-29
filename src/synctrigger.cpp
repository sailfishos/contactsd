/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2014 - 2019 Jolla Ltd
 ** Copyright (c) 2020 Open Mobile Platform LLC.
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#include "synctrigger.h"
#include "debug.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>

// buteo-syncfw
#include <ProfileEngineDefs.h>
#include <SyncProfile.h>
#include <SyncCommonDefs.h>
#include <ProfileManager.h>

namespace Contactsd {

SyncTrigger::SyncTrigger(QDBusConnection *connection)
    : mDBusConnection(connection)
    , mHaveRegisteredDBus(false)
{
}

SyncTrigger::~SyncTrigger()
{
    if (mHaveRegisteredDBus) {
        mDBusConnection->unregisterObject(QLatin1String("/SyncTrigger"));
    }
}

bool SyncTrigger::registerTriggerService()
{
    if (mHaveRegisteredDBus) {
        return true;
    }

    if (!mDBusConnection->registerObject(QLatin1String("/SyncTrigger"), this, QDBusConnection::ExportAllContents)) {
        qCWarning(lcContactsd) << "Could not register DBus object '/SyncTrigger':" << mDBusConnection->lastError();
        return false;
    }

    mHaveRegisteredDBus = true;
    return true;
}

void SyncTrigger::triggerSync(const QStringList &accountProviders, int syncPolicy, int directionPolicy)
{
    // TODO: in the future, handle AdaptivePolicy by coalescing triggered syncs into time segments.
    qCDebug(lcContactsd) << "triggering sync:" << accountProviders << syncPolicy << directionPolicy;

    // We trigger sync with a particular Buteo sync profile if:
    //  - the profile is a per-account (not template) profile
    //  - the profile is enabled
    //  - the profile is for the service corresponding to the sync target
    //  - the profile is a Contacts sync profile
    //  - the profile has two-way directionality (or upsync), or the direction policy permits
    //  - the profile has always-up-to-date set (sync on change), or the sync policy permits
    Buteo::ProfileManager profileManager;
    QList<Buteo::SyncProfile*> syncProfiles = profileManager.allSyncProfiles();
    Q_FOREACH (Buteo::SyncProfile *profile, syncProfiles) {
        if (!profile) {
            continue;
        }

        QString profileId = profile->name();
        bool notTemplate = !profile->key(Buteo::KEY_ACCOUNT_ID, QString()).isEmpty()
                         && profile->key(Buteo::KEY_ACCOUNT_ID, QStringLiteral("0")) != QStringLiteral("0");
        bool isEnabled = profile->isEnabled();
        bool isUpsync = profile->syncDirection() == Buteo::SyncProfile::SYNC_DIRECTION_TWO_WAY
                     || profile->syncDirection() == Buteo::SyncProfile::SYNC_DIRECTION_TO_REMOTE;
        bool alwaysUpToDate = profile->key(Buteo::KEY_SYNC_ALWAYS_UP_TO_DATE, QStringLiteral("false")) == QStringLiteral("true");

        // By convention, the template profile name should be of the form:
        // "accountProvider.dataType" -- eg, "google.Contacts"
        // And per-account profiles should be suffixed with "-accountId"
        bool isContacts = profileId.toLower().contains(QStringLiteral("contacts"));
        bool isTarget = accountProviders.isEmpty();
        if (!isTarget) {
            Q_FOREACH (const QString &accountProvider, accountProviders) {
                if (profileId.toLower().startsWith(accountProvider.toLower())) {
                    isTarget = true;
                    break;
                }
            }
        }

        // in the future, we should inspect the sync profile for deltasync flag
        if (!profileId.toLower().startsWith(QStringLiteral("google"))
                && !profileId.toLower().startsWith(QStringLiteral("carddav"))) {
            isTarget = false; // we currently only support automatic sync with Google and CardDAV
        }

        delete profile;

        if (notTemplate && isEnabled && isTarget && isContacts
                && (isUpsync || directionPolicy == SyncTrigger::AnyDirection)
                && (alwaysUpToDate || syncPolicy == SyncTrigger::ForceSync)) {
            qCDebug(lcContactsd) << "SyncTrigger: profile meets criteria, triggering:" << profileId;
            QDBusMessage message = QDBusMessage::createMethodCall(
                    QStringLiteral("com.meego.msyncd"),
                    QStringLiteral("/synchronizer"),
                    QStringLiteral("com.meego.msyncd"),
                    QStringLiteral("startSync"));
            message.setArguments(QVariantList() << profileId);
            QDBusConnection::sessionBus().asyncCall(message);
        }
    }
}

}

