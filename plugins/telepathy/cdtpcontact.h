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

#include <QObject>

#include <TelepathyQt4/Contact>
#include <TelepathyQt4/Types>

#include "types.h"

class CDTpContact : public QObject, public Tp::RefCounted
{
    Q_OBJECT

public:
    enum Change {
        Alias         = (1 << 0),
        Presence      = (1 << 1),
        Capabilities  = (1 << 2),
        Avatar        = (1 << 3),
        Authorization = (1 << 4),
        Infomation    = (1 << 5),
        Blocked       = (1 << 6),
        Visibility    = (1 << 7),
        All           = (1 << 8) -1
    };
    Q_DECLARE_FLAGS(Changes, Change)

     CDTpContact(Tp::ContactPtr contact, CDTpAccount *accountWrapper);
    ~CDTpContact();

    Tp::ContactPtr contact() const { return mContact; }

    CDTpAccountPtr accountWrapper() const;
    bool isRemoved() const { return mRemoved; }
    bool isVisible() const { return mVisible; }

Q_SIGNALS:
    void changed(CDTpContactPtr contact, CDTpContact::Changes changes);

private Q_SLOTS:
    void onContactAliasChanged();
    void onContactPresenceChanged();
    void onContactCapabilitiesChanged();
    void onContactAvatarDataChanged();
    void onContactAuthorizationChanged();
    void onContactInfoChanged();
    void onBlockStatusChanged();

private:
    void emitChanged(CDTpContact::Changes changes);
    void updateVisibility();
    void setRemoved(bool value);

    friend class CDTpAccount;
    Tp::ContactPtr mContact;
    CDTpAccount *mAccountWrapper;
    bool mRemoved;
    bool mVisible;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpContact::Changes)

#endif // CDTPCONTACT_H
