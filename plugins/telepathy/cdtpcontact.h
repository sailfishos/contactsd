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

#ifndef CDTPCONTACT_H
#define CDTPCONTACT_H

#include <TelepathyQt4/Contact>
#include <TelepathyQt4/Types>

#include <QObject>

class CDTpAccount;

class CDTpContact : public QObject
{
    Q_OBJECT

public:
    enum Change {
        Alias         = 0x1,
        Presence      = 0x2,
        Capabilities  = 0x4,
        Avatar        = 0x8,
        Authorization = 0x10,
        Infomation    = 0x20,
        All           = 0xFFFF
    };
    Q_DECLARE_FLAGS(Changes, Change)

    CDTpContact(Tp::ContactPtr contact, CDTpAccount *accountWrapper);
    ~CDTpContact();

    Tp::ContactPtr contact() const { return mContact; }

    CDTpAccount *accountWrapper() const { return mAccountWrapper; }

Q_SIGNALS:
    void changed(CDTpContact *contact, CDTpContact::Changes changes);

private Q_SLOTS:
    void onContactAliasChanged();
    void onContactPresenceChanged();
    void onContactCapabilitiesChanged();
    void onContactAvatarDataChanged();
    void onContactAuthorizationChanged();
    void onContactInfoChanged();

private:
    Tp::ContactPtr mContact;
    CDTpAccount *mAccountWrapper;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpContact::Changes)

#endif // CDTPCONTACT_H
