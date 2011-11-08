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

#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Profile>

#include "cdtpaccount.h"
#include "cdtpaccountcacheloader.h"
#include "cdtpaccountcachewriter.h"
#include "cdtpcontact.h"
#include "debug.h"

static const int DisconnectGracePeriod = 30 * 1000; // ms

using namespace Contactsd;

CDTpAccount::CDTpAccount(const Tp::AccountPtr &account, const QStringList &toAvoid, bool newAccount, QObject *parent)
    : QObject(parent),
      mAccount(account),
      mContactsToAvoid(toAvoid),
      mHasRoster(false),
      mNewAccount(newAccount),
      mImporting(false)
{
    // connect all signals we care about, so we can signal that the account
    // changed accordingly
    connect(mAccount.data(),
            SIGNAL(displayNameChanged(const QString &)),
            SLOT(onAccountDisplayNameChanged()));
    connect(mAccount.data(),
            SIGNAL(nicknameChanged(const QString &)),
            SLOT(onAccountNicknameChanged()));
    connect(mAccount.data(),
            SIGNAL(currentPresenceChanged(const Tp::Presence &)),
            SLOT(onAccountCurrentPresenceChanged()));
    connect(mAccount.data(),
            SIGNAL(avatarChanged(const Tp::Avatar &)),
            SLOT(onAccountAvatarChanged()));
    connect(mAccount.data(),
            SIGNAL(connectionChanged(const Tp::ConnectionPtr &)),
            SLOT(onAccountConnectionChanged(const Tp::ConnectionPtr &)));
    connect(mAccount.data(),
            SIGNAL(stateChanged(bool)),
            SLOT(onAccountStateChanged()));

    if (not newAccount) {
        CDTpAccountCacheLoader(this).run();
    }

    setConnection(mAccount->connection());

    mDisconnectTimeout.setInterval(DisconnectGracePeriod);
    mDisconnectTimeout.setSingleShot(true);

    connect(&mDisconnectTimeout, SIGNAL(timeout()), SLOT(onDisconnectTimeout()));
}

CDTpAccount::~CDTpAccount()
{
    if (not mCurrentConnection.isNull()) {
        makeRosterCache();
    }

    CDTpAccountCacheWriter(this).run();
}

QList<CDTpContactPtr> CDTpAccount::contacts() const
{
    QList<CDTpContactPtr> contacts;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, mContacts) {
        if (contactWrapper->isVisible()) {
            contacts << contactWrapper;
        }
    }

    return contacts;
}

QHash<QString, CDTpContact::Changes> CDTpAccount::rosterChanges() const
{
    QHash<QString, CDTpContact::Changes> changes;

    QSet<QString> cachedAddresses = mRosterCache.keys().toSet();
    QSet<QString> currentAddresses;

    Q_FOREACH (CDTpContactPtr contact, contacts()) {
        const QString contactId = contact->contact()->id();
        const QHash<QString, CDTpContact::Info>::ConstIterator it = mRosterCache.find(contactId);

        currentAddresses.insert(contactId);

        if (it == mRosterCache.constEnd()) {
            qDebug() << "No cached contact for" << contactId;
            changes.insert(contactId, CDTpContact::Added);
            continue;
        }

        changes.insert(contactId, contact->info().diff(*it));
    }

    cachedAddresses.subtract(currentAddresses);

    // The remaining addresses after the subtraction are those which were in
    // the cache but are not in the contact list anymore
    Q_FOREACH (const QString &id, cachedAddresses) {
        changes.insert(id, CDTpContact::Deleted);
    }

    return changes;
}

void CDTpAccount::setContactsToAvoid(const QStringList &contactIds)
{
    mContactsToAvoid = contactIds;
    Q_FOREACH (const QString &id, contactIds) {
        CDTpContactPtr contactWrapper = mContacts.take(id);
        if (contactWrapper) {
            contactWrapper->setRemoved(true);
        }
    }
}

void CDTpAccount::onAccountDisplayNameChanged()
{
    Q_EMIT changed(CDTpAccountPtr(this), DisplayName);
}

void CDTpAccount::onAccountNicknameChanged()
{
    Q_EMIT changed(CDTpAccountPtr(this), Nickname);
}

void CDTpAccount::onAccountCurrentPresenceChanged()
{
    Q_EMIT changed(CDTpAccountPtr(this), Presence);
}

void CDTpAccount::onAccountAvatarChanged()
{
    Q_EMIT changed(CDTpAccountPtr(this), Avatar);
}

void CDTpAccount::onAccountStateChanged()
{
    Q_EMIT changed(CDTpAccountPtr(this), Enabled);

    if (!isEnabled()) {
        setConnection(Tp::ConnectionPtr());
        mRosterCache.clear();
        CDTpAccountCacheWriter(this).run();
    } else {
        /* Since contacts got removed when we disabled the account, we need
         * to threat this account as new now that it is enabled again */
        mNewAccount = true;
    }
}

void CDTpAccount::onAccountConnectionChanged(const Tp::ConnectionPtr &connection)
{
    bool oldHasRoster = mHasRoster;

    // If we got disconnected, but not on user request, give a grace period
    // before actually updating Tracker, in case the connection comes back
    // quickly
    if (not connection.isNull()) {
        mDisconnectTimeout.stop();
    } else if (not mCurrentConnection.isNull() && mCurrentConnection->status() != Tp::ConnectionStatusDisconnected) {
        debug() << "Lost connection for account" << mAccount->objectPath()
                << ", giving a grace period of" << DisconnectGracePeriod << "ms";
        mDisconnectTimeout.start();
        return;
    }

    setConnection(connection);
    if (oldHasRoster != mHasRoster) {
        Q_EMIT rosterChanged(CDTpAccountPtr(this));
        mNewAccount = false;
    }
}

void CDTpAccount::setConnection(const Tp::ConnectionPtr &connection)
{
    debug() << "Account" << mAccount->objectPath() << "- has connection:" << (connection != 0);

    if (not mCurrentConnection.isNull()) {
        makeRosterCache();
    }

    mContacts.clear();
    mHasRoster = false;
    mCurrentConnection = connection;

    if (connection) {
        /* If the connection has no roster, no need to bother with sync signals */
        if (not (connection->actualFeatures().contains(Tp::Connection::FeatureRoster))) {
            debug() << "Account" << mAccount->objectPath() << "has no roster, not emitting sync signals";

            return;
        }

        /* If this is the first time account gets a connection, report we are
         * importing. Note that connection could still be CONNECTING so it is
         * not sure we'll actually import anything. */
        if (mNewAccount) {
            mImporting = true;
            Q_EMIT syncStarted(mAccount);
        }

        connect(connection->contactManager().data(),
                SIGNAL(stateChanged(Tp::ContactListState)),
                SLOT(onContactListStateChanged(Tp::ContactListState)));
        setContactManager(connection->contactManager());
    } else if (mImporting) {
        /* We lost the connection while importing, probably failed to connect,
         * report 0 contacts retrieved */
        emitSyncEnded(0, 0);
    }
}

void CDTpAccount::onDisconnectTimeout()
{
    // Reset the current connection, and call onAccountConnectionChanged with
    // a null connection again to actually put the account offline
    mCurrentConnection = Tp::ConnectionPtr();
    onAccountConnectionChanged(Tp::ConnectionPtr());
}

void CDTpAccount::onContactListStateChanged(Tp::ContactListState state)
{
    Q_UNUSED(state);

    /* NB#240743 - It can happen that tpqt4 still emits signals on the
     * ContactManager after connection has been removed from the account.
     * In that case Tp::Account::connectionChanged() should be emitted soon */
    if (!mAccount->connection()) {
        return;
    }

    bool oldHasRoster = mHasRoster;
    setContactManager(mAccount->connection()->contactManager());
    if (oldHasRoster != mHasRoster) {
        Q_EMIT rosterChanged(CDTpAccountPtr(this));
        mNewAccount = false;
    }
}

void CDTpAccount::setContactManager(const Tp::ContactManagerPtr &contactManager)
{
    if (contactManager->state() != Tp::ContactListStateSuccess) {
        return;
    }

    if (mHasRoster) {
        warning() << "Account" << mAccount->objectPath() << "- already received the roster";
        return;
    }

    debug() << "Account" << mAccount->objectPath() << "- received the roster";

    mHasRoster = true;
    connect(contactManager.data(),
            SIGNAL(allKnownContactsChanged(const Tp::Contacts &, const Tp::Contacts &, const Tp::Channel::GroupMemberChangeDetails &)),
            SLOT(onAllKnownContactsChanged(const Tp::Contacts &, const Tp::Contacts &)));

    Q_FOREACH (const Tp::ContactPtr &contact, contactManager->allKnownContacts()) {
        if (mContactsToAvoid.contains(contact->id())) {
            continue;
        }
        insertContact(contact);
        if (mNewAccount) {
            maybeRequestExtraInfo(contact);
        }
    }
}

void CDTpAccount::emitSyncEnded(int contactsAdded, int contactsRemoved)
{
    if (mImporting) {
        mImporting = false;
        Q_EMIT syncEnded(mAccount, contactsAdded, contactsRemoved);
    }
}

QHash<QString, CDTpContact::Info> CDTpAccount::rosterCache() const
{
    return mRosterCache;
}

void CDTpAccount::setRosterCache(const QHash<QString, CDTpContact::Info> &cache)
{
    mRosterCache = cache;
}

void CDTpAccount::onAllKnownContactsChanged(const Tp::Contacts &contactsAdded,
        const Tp::Contacts &contactsRemoved)
{
    debug() << "Account" << mAccount->objectPath() << "roster contacts changed:";
    debug() << " " << contactsAdded.size() << "contacts added";
    debug() << " " << contactsRemoved.size() << "contacts removed";

    QList<CDTpContactPtr> added;
    Q_FOREACH (const Tp::ContactPtr &contact, contactsAdded) {
        if (mContacts.contains(contact->id())) {
            warning() << "Internal error, contact was already in roster";
            continue;
        }
        if (mContactsToAvoid.contains(contact->id())) {
            continue;
        }
        maybeRequestExtraInfo(contact);
        CDTpContactPtr contactWrapper = insertContact(contact);
        if (contactWrapper->isVisible()) {
            added << contactWrapper;
        }
    }

    QList<CDTpContactPtr> removed;
    Q_FOREACH (const Tp::ContactPtr &contact, contactsRemoved) {
        const QString id(contact->id());
        if (!mContacts.contains(id)) {
            warning() << "Internal error, contact is not in the internal list"
                "but was removed from roster";
            continue;
        }
        CDTpContactPtr contactWrapper = mContacts.take(id);
        if (contactWrapper->isVisible()) {
            removed << contactWrapper;
        }
        contactWrapper->setRemoved(true);
    }

    if (!added.isEmpty() || !removed.isEmpty()) {
        Q_EMIT rosterUpdated(CDTpAccountPtr(this), added, removed);
    }
}

void CDTpAccount::onAccountContactChanged(CDTpContactPtr contactWrapper,
        CDTpContact::Changes changes)
{
    if ((changes & CDTpContact::Visibility) != 0) {
        // Visibility of this contact changed. Transform this update operation
        // to an add/remove operation
        debug() << "Visibility changed for contact" << contactWrapper->contact()->id();

        QList<CDTpContactPtr> added;
        QList<CDTpContactPtr> removed;
        if (contactWrapper->isVisible()) {
            added << contactWrapper;
        } else {
            removed << contactWrapper;
        }

        Q_EMIT rosterUpdated(CDTpAccountPtr(this), added, removed);

        return;
    }

    // Forward changes only if contact is visible
    if (contactWrapper->isVisible()) {
        Q_EMIT rosterContactChanged(contactWrapper, changes);
    }
}

CDTpContactPtr CDTpAccount::insertContact(const Tp::ContactPtr &contact)
{
    debug() << "  creating wrapper for contact" << contact->id();

    CDTpContactPtr contactWrapper = CDTpContactPtr(new CDTpContact(contact, this));
    connect(contactWrapper.data(),
            SIGNAL(changed(CDTpContactPtr, CDTpContact::Changes)),
            SLOT(onAccountContactChanged(CDTpContactPtr, CDTpContact::Changes)));
    mContacts.insert(contact->id(), contactWrapper);
    return contactWrapper;
}

void CDTpAccount::maybeRequestExtraInfo(Tp::ContactPtr contact)
{
    if (!contact->isAvatarTokenKnown()) {
        debug() << contact->id() << "first seen: request avatar";
        contact->requestAvatarData();
    }
    if (!contact->isContactInfoKnown()) {
        debug() << contact->id() << "first seen: refresh ContactInfo";
        contact->refreshInfo();
    }
}

void CDTpAccount::makeRosterCache()
{
    mRosterCache.clear();

    Q_FOREACH (const CDTpContactPtr &ptr, mContacts) {
        mRosterCache.insert(ptr->contact()->id(), ptr->info());
    }
}

CDTpContactPtr CDTpAccount::contact(const QString &id) const
{
    return mContacts.value(id);
}

