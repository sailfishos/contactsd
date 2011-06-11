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
        Information    = (1 << 5),
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
    bool isAvatarKnown() const;
    bool isInformationKnown() const;

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
    void onQueuedChangesTimeout();

private:
    void emitChanged(CDTpContact::Changes changes);
    void updateVisibility();
    void setRemoved(bool value);

    friend class CDTpAccount;
    Tp::ContactPtr mContact;
    QPointer<CDTpAccount> mAccountWrapper;
    bool mRemoved;
    bool mVisible;
    Changes mQueuedChanges;
    QTimer mQueuedChangesTimer;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CDTpContact::Changes)

#endif // CDTPCONTACT_H
