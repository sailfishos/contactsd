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
    : QObject(accountWrapper),
      mContact(contact),
      mAccountWrapper(accountWrapper)
{
    connect(contact.data(),
            SIGNAL(aliasChanged(const QString &)),
            SLOT(onContactAliasChanged()));
    connect(contact.data(),
            SIGNAL(simplePresenceChanged(const QString &status, uint type,
                    const QString &presenceMessage)),
            SLOT(onContactPresenceChanged()));
    connect(contact.data(),
            SIGNAL(capabilitiesChanged(Tp::ContactCapabilities *)),
            SLOT(onContactCapabilitiesChanged()));
    // TODO: add avatar support
    // connect(contact.data(),
    //         SIGNAL(avatarDataChanged(const Tp::AvatarData &)),
    //         SLOT(onContactAvatarDataChanged(const Tp::AvatarData &)));
}

CDTpContact::~CDTpContact()
{
}

void CDTpContact::onContactAliasChanged()
{
    emit changed(this, Alias);
}

void CDTpContact::onContactPresenceChanged()
{
    emit changed(this, Alias);
}

void CDTpContact::onContactCapabilitiesChanged()
{
    emit changed(this, Capabilities);
}

// TODO: add avatar support
// void CDTpContact::onContactAvatarDataChanged()
// {
//     emit changed(this, Avatar);
// }
