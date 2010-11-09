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
            SIGNAL(simplePresenceChanged(const QString &, uint, const QString &)),
            SLOT(onContactPresenceChanged()));
    connect(contact.data(),
            SIGNAL(capabilitiesChanged(Tp::ContactCapabilities *)),
            SLOT(onContactCapabilitiesChanged()));
    connect(contact.data(),
            SIGNAL(avatarDataChanged(const Tp::AvatarData &)),
            SLOT(onContactAvatarDataChanged()));
    connect(contact.data(),
            SIGNAL(subscriptionStateChanged(Tp::Contact::PresenceState)),
            SLOT(onContactAuthorizationChanged()));
    connect(contact.data(),
            SIGNAL(publishStateChanged(Tp::Contact::PresenceState)),
            SLOT(onContactAuthorizationChanged()));
    connect(contact.data(),
            SIGNAL(infoChanged(const Tp::ContactInfoFieldList &)),
            SLOT(onContactInfoChanged()));
}

CDTpContact::~CDTpContact()
{
}

void CDTpContact::onContactAliasChanged()
{
    emit changed(CDTpContactPtr(this), Alias);
}

void CDTpContact::onContactPresenceChanged()
{
    emit changed(CDTpContactPtr(this), Presence);
}

void CDTpContact::onContactCapabilitiesChanged()
{
    emit changed(CDTpContactPtr(this), Capabilities);
}

void CDTpContact::onContactAvatarDataChanged()
{
    emit changed(CDTpContactPtr(this), Avatar);
}

void CDTpContact::onContactAuthorizationChanged()
{
    emit changed(CDTpContactPtr(this), Authorization);
}

void CDTpContact::onContactInfoChanged()
{
    emit changed(CDTpContactPtr(this), Infomation);
}
