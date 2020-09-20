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
 **/

#include "cdexportercontroller.h"

#include <contactmanagerengine.h>
#include <qtcontacts-extensions_manager_impl.h>

#include <QContactCollection>

#include <Accounts/Account>
#include <Accounts/Manager>

#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

namespace {

QMap<QString, QString> privilegedManagerParameters()
{
    QMap<QString, QString> rv;
    rv.insert(QStringLiteral("mergePresenceChanges"), QStringLiteral("false"));
    return rv;
}

QString managerName()
{
    return QStringLiteral("org.nemomobile.contacts.sqlite");
}

}

CDExporterController::CDExporterController(QObject *parent)
    : QObject(parent)
    , m_privilegedManager(managerName(), privilegedManagerParameters())
{
    QtContactsSqliteExtensions::ContactManagerEngine *engine
            = QtContactsSqliteExtensions::contactManagerEngine(m_privilegedManager);
    connect(engine, &QtContactsSqliteExtensions::ContactManagerEngine::collectionContactsChanged,
            this, &CDExporterController::collectionContactsChanged);
}

CDExporterController::~CDExporterController()
{
}

void CDExporterController::collectionContactsChanged(const QList<QContactCollectionId> &collectionIds)
{
    // If the collection originates from an account (e.g. an address book synced from a remote cloud
    // account) then sync those changes upstream to that account.

    QStringList accountProviders;
    for (const QContactCollectionId &collectionId : collectionIds) {
        const QContactCollection collection = m_privilegedManager.collection(collectionId);
        const Accounts::AccountId accountId = collection.extendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_ACCOUNTID).toInt();
        if (accountId > 0) {
            if (!m_manager) {
                m_manager = new Accounts::Manager(this);
            }
            Accounts::Account *account = m_manager->account(accountId);
            if (account) {
                accountProviders.append(account->providerName());
            } else {
                qWarning() << "CDExport: got change notification for contact collection" << collectionId
                           << "matching account id" << accountId << "but cannot find matching account!";
            }
        }
    }

    if (!accountProviders.isEmpty()) {
        qWarning() << "CDExport: triggering contacts remote sync:" << accountProviders;
        QDBusMessage message = QDBusMessage::createMethodCall(
                QStringLiteral("com.nokia.contactsd"),
                QStringLiteral("/SyncTrigger"),
                QStringLiteral("com.nokia.contactsd"),
                QStringLiteral("triggerSync"));
        message.setArguments(QVariantList()
                << QVariant::fromValue<QStringList>(accountProviders)
                << QVariant::fromValue<qint32>(1)   // only if AlwaysUpToDate set in profile
                << QVariant::fromValue<qint32>(1)); // only if Upsync or TwoWay direction
        QDBusConnection::sessionBus().asyncCall(message);
    }
}
