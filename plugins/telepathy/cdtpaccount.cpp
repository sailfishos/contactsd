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

#include "cdtpaccount.h"

#include "cdtpcontact.h"

#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Profile>

#include <QDebug>

CDTpAccount::CDTpAccount(const Tp::AccountPtr &account, QObject *parent)
    : QObject(parent),
      mAccount(account),
      mIntrospectingRoster(false),
      mRosterReady(false)
{
    qDebug() << "Introspecting account" << account->objectPath();
    connect(mAccount->becomeReady(
            Tp::Features() << Tp::Account::FeatureCore
                           << Tp::Account::FeatureAvatar
                           << Tp::Account::FeatureCapabilities),
            SIGNAL(finished(Tp::PendingOperation *)),
            SLOT(onAccountReady(Tp::PendingOperation *)));
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
    if (profile != 0) {
        return profile->provider();
    }

    return mAccount->serviceName();
}

void CDTpAccount::onAccountReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qDebug() << "Could not make account" <<  mAccount->objectPath() <<
            "ready:" << op->errorName() << "-" << op->errorMessage();
        // TODO signal error
        return;
    }

    qDebug() << "Account" << mAccount->objectPath() << "ready";

    // signal that the account is ready to use
    Q_EMIT ready(CDTpAccountPtr(this));

    // connect all signals we care about, so we can signal that the account
    // changed accordingly
    connect(mAccount.data(),
            SIGNAL(normalizedNameChanged(const QString &)),
            SLOT(onAccountNormalizedNameChanged()));
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
    if (mAccount->connection()) {
        introspectAccountConnection();
    }
}

void CDTpAccount::onAccountNormalizedNameChanged()
{
    Q_EMIT changed(CDTpAccountPtr(this), All);
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
    qDebug() << "Account" << mAccount->objectPath() << "connection changed";

    if (mRosterReady) {
        // Account::connectionChanged is emited every time the account
        // connection changes, so always clear contacts we have from the
        // old connection
        clearContacts();

        // let's emit rosterChanged(false), to inform that right now we don't
        // have a roster configured
        mRosterReady = false;
        Q_EMIT rosterChanged(CDTpAccountPtr(this), false);
    }

    // if we have a new connection, introspect it
    if (connection) {
        introspectAccountConnection();
    }
}

void CDTpAccount::onAccountConnectionReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qDebug() << "Could not make account" << mAccount->objectPath() <<
            "connection ready:" << op->errorName() << "-" << op->errorMessage();
        return;
    }

    qDebug() << "Account" << mAccount->objectPath() << "connection ready";

    Tp::ConnectionPtr connection = mAccount->connection();
    connect(connection.data(),
        SIGNAL(statusChanged(Tp::ConnectionStatus)),
        SLOT(onAccountConnectionStatusChanged(Tp::ConnectionStatus)));
    if (connection->status() == Tp::ConnectionStatusConnected) {
        introspectAccountConnectionRoster();
    }
}

void CDTpAccount::onAccountConnectionStatusChanged(Tp::ConnectionStatus status)
{
    if (status == Tp::ConnectionStatusConnected) {
        introspectAccountConnectionRoster();
    }
}

void CDTpAccount::onAccountConnectionRosterReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qWarning() << "Could not make account" <<  mAccount->objectPath() <<
            "roster ready:" << op->errorName() << "-" << op->errorMessage();
        return;
    }

    Tp::ConnectionPtr connection = mAccount->connection();
    Tp::ContactManagerPtr contactManager = connection->contactManager();
    connect(contactManager.data(),
            SIGNAL(allKnownContactsChanged(const Tp::Contacts &, const Tp::Contacts &, const Tp::Channel::GroupMemberChangeDetails &)),
            SLOT(onAccountContactsChanged(const Tp::Contacts &, const Tp::Contacts &)));
    upgradeContacts(contactManager->allKnownContacts());
}

void CDTpAccount::onAccountContactsUpgraded(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qDebug() << "Could not upgrade account" << mAccount->objectPath() <<
            "roster contacts";
        return;
    }

    qDebug() << "Account" << mAccount->objectPath() <<
        "roster contacts upgraded";

    Tp::PendingContacts *pc = qobject_cast<Tp::PendingContacts *>(op);
    QList<CDTpContactPtr> added;
    Q_FOREACH (const Tp::ContactPtr &contact, pc->contacts()) {
        qDebug() << "  creating wrapper for contact" << contact->id();
        CDTpContactPtr contactWrapper = insertContact(contact);
        if (contactWrapper->isVisible()) {
            added.append(contactWrapper);
        }
    }
    if (!mRosterReady) {
        qDebug() << "Account" << mAccount->objectPath() <<
            "roster ready";

        mIntrospectingRoster = false;
        mRosterReady = true;
        Q_EMIT rosterChanged(CDTpAccountPtr(this), true);
    } else {
        Q_EMIT rosterUpdated(CDTpAccountPtr(this), added, QList<CDTpContactPtr>());
    }
}

void CDTpAccount::onAccountContactsChanged(const Tp::Contacts &contactsAdded,
        const Tp::Contacts &contactsRemoved)
{
    qDebug() << "Account" << mAccount->objectPath() <<
        "roster contacts changed:";
    qDebug() << " " << contactsAdded.size() << "contacts added";
    qDebug() << " " << contactsRemoved.size() << "contacts removed";

    // delay emission of rosterUpdated with contactsAdded until the contacts are
    // upgraded
    if (!contactsAdded.isEmpty()) {
        upgradeContacts(contactsAdded);
        qDebug() << "Account" << mAccount->objectPath() << "- delaying "
            "contactsUpdated signal for added contacts until they are upgraded";
    }

    QList<CDTpContactPtr> removed;
    Q_FOREACH (const Tp::ContactPtr &contact, contactsRemoved) {
        if (!mContacts.contains(contact)) {
            qWarning() << "Internal error, contact is not in the internal list"
                "but was removed from roster";
            continue;
        }
        CDTpContactPtr contactWrapper = mContacts.take(contact);
        if (contactWrapper->isVisible()) {
            removed.append(contactWrapper);
        }
        contactWrapper->setRemoved(true);
    }

    Q_EMIT rosterUpdated(CDTpAccountPtr(this), QList<CDTpContactPtr>(), removed);
}

void CDTpAccount::onAccountContactChanged(CDTpContactPtr contactWrapper,
        CDTpContact::Changes changes)
{
    if ((changes & CDTpContact::Visibility) != 0) {
        // Visibility of this contact changed. Transform this update operation
        // to an add/remove operation
        qDebug() << "Visibility changed for contact" << contactWrapper->contact()->id();
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
        Q_EMIT rosterContactChanged(CDTpAccountPtr(this), contactWrapper, changes);
    }
}

void CDTpAccount::introspectAccountConnection()
{
    qDebug() << "Introspecting account" << mAccount->objectPath() <<
        "connection";

    Tp::ConnectionPtr connection = mAccount->connection();
    connect(connection->becomeReady(Tp::Connection::FeatureCore),
            SIGNAL(finished(Tp::PendingOperation *)),
            SLOT(onAccountConnectionReady(Tp::PendingOperation *)));
}

void CDTpAccount::introspectAccountConnectionRoster()
{
    if (mIntrospectingRoster || mRosterReady) {
        return;
    }

    qDebug() << "Introspecting account" << mAccount->objectPath() <<
        "roster";

    mIntrospectingRoster = true;
    Tp::ConnectionPtr connection = mAccount->connection();
    // TODO: add support to roster groups?
    connect(connection->becomeReady(Tp::Connection::FeatureRoster),
            SIGNAL(finished(Tp::PendingOperation *)),
            SLOT(onAccountConnectionRosterReady(Tp::PendingOperation *)));
}

void CDTpAccount::upgradeContacts(const Tp::Contacts &contacts)
{
    qDebug() << "Upgrading account" << mAccount->objectPath() <<
        "roster contacts with desired features";
    Tp::ConnectionPtr connection = mAccount->connection();
    Tp::ContactManagerPtr contactManager = connection->contactManager();
    Tp::PendingContacts *pc = contactManager->upgradeContacts(contacts.toList(),
            Tp::Features() <<
                Tp::Contact::FeatureAlias <<
                Tp::Contact::FeatureAvatarToken <<
                Tp::Contact::FeatureAvatarData <<
                Tp::Contact::FeatureSimplePresence <<
                Tp::Contact::FeatureInfo <<
                Tp::Contact::FeatureLocation <<
                Tp::Contact::FeatureCapabilities);
    connect(pc,
            SIGNAL(finished(Tp::PendingOperation *)),
            SLOT(onAccountContactsUpgraded(Tp::PendingOperation *)));
}

CDTpContactPtr CDTpAccount::insertContact(const Tp::ContactPtr &contact)
{
    CDTpContactPtr contactWrapper = CDTpContactPtr(new CDTpContact(contact, this));
    connect(contactWrapper.data(),
            SIGNAL(changed(CDTpContactPtr, CDTpContact::Changes)),
            SLOT(onAccountContactChanged(CDTpContactPtr, CDTpContact::Changes)));
    mContacts.insert(contact, contactWrapper);
    return contactWrapper;
}

void CDTpAccount::clearContacts()
{
    mContacts.clear();
}
