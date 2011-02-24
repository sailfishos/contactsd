/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (people-users@projects.maemo.org)
**
** This file is part of contactsd.
**
** If you have questions regarding the use of this file, please contact
** Nokia at people-users@projects.maemo.org.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Profile>

#include "cdtpaccount.h"
#include "cdtpcontact.h"
#include "debug.h"

using namespace Contactsd;

CDTpAccount::CDTpAccount(const Tp::AccountPtr &account, QObject *parent)
    : QObject(parent),
      mAccount(account),
      mHasRoster(false),
      mFirstSeen(false)
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

    setConnection(mAccount->connection());
}

CDTpAccount::~CDTpAccount()
{
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

QString CDTpAccount::providerName() const
{
    Tp::ProfilePtr profile = mAccount->profile();
    if (profile != 0 && !profile->provider().isEmpty()) {
        return profile->provider();
    }

    return mAccount->serviceName();
}

void CDTpAccount::firstTimeSeen()
{
    Q_FOREACH (const CDTpContactPtr &contactWrapper, mContacts.values()) {
        maybeRequestExtraInfo(contactWrapper->contact());
    }
    if (!mAccount->connection()) {
        mFirstSeen = true;
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

void CDTpAccount::onAccountConnectionChanged(const Tp::ConnectionPtr &connection)
{
    bool oldHasRoster = mHasRoster;
    setConnection(connection);
    if (oldHasRoster != mHasRoster) {
        Q_EMIT rosterChanged(CDTpAccountPtr(this));
    }
}

void CDTpAccount::setConnection(const Tp::ConnectionPtr &connection)
{
    debug() << "Account" << mAccount->objectPath() << "- has connection:" << (connection != 0);

    mContacts.clear();
    mHasRoster = false;

    if (connection && connection->actualFeatures().contains(Tp::Connection::FeatureRoster)) {
        connect(connection->contactManager().data(),
                SIGNAL(stateChanged(Tp::ContactListState)),
                SLOT(onStateChanged(Tp::ContactListState)));
        onStateChanged(connection->contactManager()->state());
    }
}

void CDTpAccount::onStateChanged(Tp::ContactListState state)
{
    if (state != Tp::ContactListStateSuccess) {
        return;
    }

    if (mHasRoster) {
        warning() << "Already got the roster";
        return;
    }

    mHasRoster = true;
    Tp::ContactManagerPtr contactManager = mAccount->connection()->contactManager();
    connect(contactManager.data(),
            SIGNAL(allKnownContactsChanged(const Tp::Contacts &, const Tp::Contacts &, const Tp::Channel::GroupMemberChangeDetails &)),
            SLOT(onAllKnownContactsChanged(const Tp::Contacts &, const Tp::Contacts &)));

    Q_FOREACH (const Tp::ContactPtr &contact, contactManager->allKnownContacts()) {
        insertContact(contact);
        if (mFirstSeen) {
            maybeRequestExtraInfo(contact);
        }
    }

    mFirstSeen = false;
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
        if (contactWrapper->isVisible()) {
            Q_EMIT rosterUpdated(CDTpAccountPtr(this),
                QList<CDTpContactPtr>() << contactWrapper,
                QList<CDTpContactPtr>());
        } else {
            contactWrapper->setRemoved(true);
            Q_EMIT rosterUpdated(CDTpAccountPtr(this),
                QList<CDTpContactPtr>(),
                QList<CDTpContactPtr>() << contactWrapper);
        }

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

CDTpContactPtr CDTpAccount::contact(const QString &id) const
{
    return mContacts.value(id);
}

