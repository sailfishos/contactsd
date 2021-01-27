/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2014 - 2021 Jolla Ltd
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

#include <Accounts/Service>
#include <Accounts/AccountService>

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

bool accountIsEnabled(Accounts::Account *account)
{
    Accounts::Service srv;
    const Accounts::ServiceList &services = account->services();
    for (const Accounts::Service &s : services) {
        if (s.serviceType().toLower() == QStringLiteral("carddav")
                || s.name().toLower().contains("contacts")) {
            srv = s;
            break;
        }
    }

    Accounts::AccountService globalSrv(account, Accounts::Service());
    if (srv.isValid()) {
        Accounts::AccountService accSrv(account, srv);
        return globalSrv.isEnabled() && accSrv.isEnabled();
    } else {
        return globalSrv.isEnabled();
    }
}

void purgeAccountContacts(int accountId, QtContactsSqliteExtensions::ContactManagerEngine *cme)
{
    QContactManager::Error err = QContactManager::NoError;
    QList<QContactCollection> added, modified, deleted, unmodified;
    if (!cme->fetchCollectionChanges(accountId, QString(), &added, &modified, &deleted, &unmodified, &err)) {
        qWarning() << "Unable to retrieve contact collections for disabled account: " << accountId;
        return;
    }

    const QList<QContactCollection> all = added + modified + deleted + unmodified;
    QList<QContactCollectionId> purge;
    for (const QContactCollection &col : all) {
        purge.append(col.id());
    }

    if (purge.size() && !cme->storeChanges(nullptr, nullptr, purge,
            QtContactsSqliteExtensions::ContactManagerEngine::PreserveLocalChanges,
            true, &err)) {
        qWarning() << "Unable to purge contact collections for disabled account: " << accountId;
        return;
    }
}

}

CDExporterController::CDExporterController(QObject *parent)
    : QObject(parent)
    , m_privilegedManager(managerName(), privilegedManagerParameters())
    , m_manager(new Accounts::Manager(this))
{
    QtContactsSqliteExtensions::ContactManagerEngine *engine
            = QtContactsSqliteExtensions::contactManagerEngine(m_privilegedManager);
    connect(engine, &QtContactsSqliteExtensions::ContactManagerEngine::collectionContactsChanged,
            this, &CDExporterController::collectionContactsChanged);

    connect(&m_privilegedManager, &QContactManager::collectionsAdded,
            this, &CDExporterController::collectionsAdded);

    connect(m_manager, &Accounts::Manager::accountUpdated,
            this, &CDExporterController::accountUpdated);
    connect(m_manager, &Accounts::Manager::enabledEvent,
            this, &CDExporterController::accountUpdated);

    // load all collections and build our mappings:
    // collection to accountId, and accountId to collections.
    const QList<QContactCollection> collections = m_privilegedManager.collections();
    for (const QContactCollection &col : collections) {
        const Accounts::AccountId accountId = col.extendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_ACCOUNTID).toInt();
        m_accountCollections.insertMulti(accountId, col.id());
        m_collectionAccount.insert(col.id(), accountId);
    }
}

CDExporterController::~CDExporterController()
{
}

void CDExporterController::accountUpdated(const Accounts::AccountId &accountId)
{
    if (accountId > 0 && m_accountCollections.contains(accountId)) {
        Accounts::Account *account = m_manager->account(accountId);
        if (account) {
            // if the account is disabled (or the contact sync service for the account is disabled)
            // then we need to purge any associated contact collections.
            if (!accountIsEnabled(account)) {
                purgeAccountContacts(accountId, QtContactsSqliteExtensions::contactManagerEngine(m_privilegedManager));
            }
        }
    }
}

void CDExporterController::collectionsAdded(const QList<QContactCollectionId> &collectionIds)
{
    for (const QContactCollectionId &cid : collectionIds) {
        const QContactCollection col = m_privilegedManager.collection(cid);
        const Accounts::AccountId accountId = col.extendedMetaData(COLLECTION_EXTENDEDMETADATA_KEY_ACCOUNTID).toInt();
        m_accountCollections.insertMulti(accountId, cid);
        m_collectionAccount.insert(cid, accountId);
    }
}

void CDExporterController::collectionContactsChanged(const QList<QContactCollectionId> &collectionIds)
{
    // If the collection originates from an account (e.g. an address book synced from a remote cloud
    // account) then sync those changes upstream to that account.

    QStringList accountProviders;
    for (const QContactCollectionId &collectionId : collectionIds) {
        const Accounts::AccountId accountId = m_collectionAccount.value(collectionId);
        if (accountId > 0) {
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
