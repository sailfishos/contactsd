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

#include "cdtpcontact.h"

#include "cdtpaccount.h"

#include <TelepathyQt4/Types>

CDTpContact::CDTpContact(Tp::ContactPtr contact, CDTpAccount *accountWrapper)
    : QObject(),
      mContact(contact),
      mAccountWrapper(accountWrapper),
      mRemoved(false)
{
    connect(contact.data(),
            SIGNAL(aliasChanged(const QString &)),
            SLOT(onContactAliasChanged()));
    connect(contact.data(),
            SIGNAL(presenceChanged(const Tp::Presence &)),
            SLOT(onContactPresenceChanged()));
    connect(contact.data(),
            SIGNAL(capabilitiesChanged(const Tp::ContactCapabilities &)),
            SLOT(onContactCapabilitiesChanged()));
    connect(contact.data(),
            SIGNAL(avatarDataChanged(const Tp::AvatarData &)),
            SLOT(onContactAvatarDataChanged()));
    connect(contact.data(),
            SIGNAL(subscriptionStateChanged(Tp::Contact::PresenceState,
                    const Tp::Channel::GroupMemberChangeDetails &)),
            SLOT(onContactAuthorizationChanged()));
    connect(contact.data(),
            SIGNAL(publishStateChanged(Tp::Contact::PresenceState,
                    const Tp::Channel::GroupMemberChangeDetails &)),
            SLOT(onContactAuthorizationChanged()));
    connect(contact.data(),
            SIGNAL(infoFieldsChanged(const Tp::Contact::InfoFields &)),
            SLOT(onContactInfoChanged()));
    connect(contact.data(),
            SIGNAL(blockStatusChanged(bool)),
            SLOT(onBlockStatusChanged()));
}

CDTpContact::~CDTpContact()
{
}

void CDTpContact::onContactAliasChanged()
{
    Q_EMIT changed(CDTpContactPtr(this), Alias);
}

void CDTpContact::onContactPresenceChanged()
{
    Q_EMIT changed(CDTpContactPtr(this), Presence);
}

void CDTpContact::onContactCapabilitiesChanged()
{
    Q_EMIT changed(CDTpContactPtr(this), Capabilities);
}

void CDTpContact::onContactAvatarDataChanged()
{
    Q_EMIT changed(CDTpContactPtr(this), Avatar);
}

void CDTpContact::onContactAuthorizationChanged()
{
    Q_EMIT changed(CDTpContactPtr(this), Authorization);
}

void CDTpContact::onContactInfoChanged()
{
    Q_EMIT changed(CDTpContactPtr(this), Infomation);
}

void CDTpContact::onBlockStatusChanged()
{
    Q_EMIT changed(CDTpContactPtr(this), Blocked);
}

