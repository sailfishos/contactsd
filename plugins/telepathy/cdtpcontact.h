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
#include <TelepathyQt4/Presence>
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
        All           = (1 << 8) -1,
        // Special values
        Added         = (1 << 9) - 1,
        Deleted       = 1 << 10
    };
    Q_DECLARE_FLAGS(Changes, Change)

    class InfoData;

    class Info {
    public:
        enum Capability {
            TextChats                     = (1 << 0),
            StreamedMediaCalls            = (1 << 1),
            StreamedMediaAudioCalls       = (1 << 2),
            StreamedMediaAudioVideoCalls  = (1 << 3),
            UpgradingStreamMediaCalls     = (1 << 4),
            FileTransfers                 = (1 << 5),
            StreamTubes                   = (1 << 6),
            DBusTubes                     = (1 << 7)
        };
        // Q_DECLARE_FLAGS does not work for nested classes
        typedef int Capabilities;

    public:
        Info();
        Info(const CDTpContact *contact);

        Info(const Info &other);
        Info& operator=(const Info &other);

        ~Info();

    public:
        QString alias() const;
        void setAlias(const QString &alias);

        Tp::Presence presence() const;
        void setPresence(const Tp::Presence &presence);

        int capabilities() const;
        void setCapabilities(int capabilities);

        QString avatarPath() const;
        void setAvatarPath(const QString &avatarPath);

        bool isSubscriptionStateKnown() const;
        void setSubscriptionStateKnown(bool known);

        Tp::Contact::PresenceState subscriptionState() const;
        void setSubscriptionState(Tp::Contact::PresenceState state);

        bool isPublishStateKnown() const;
        void setPublishStateKnown(bool known);

        Tp::Contact::PresenceState publishState() const;
        void setPublishState(Tp::Contact::PresenceState state);

        bool isContactInfoKnown() const;
        void setContactInfoKnown(bool known);

        Tp::ContactInfoFieldList infoFields() const;
        void setInfoFields(const Tp::ContactInfoFieldList &fields);

        bool isVisible() const;
        void setVisible(bool visible);

        CDTpContact::Changes diff(const CDTpContact::Info &other) const;

    private:
        QSharedDataPointer<InfoData> d;
    };

     CDTpContact(Tp::ContactPtr contact, CDTpAccount *accountWrapper);
    ~CDTpContact();

    Tp::ContactPtr contact() const { return mContact; }

    CDTpAccountPtr accountWrapper() const;
    bool isRemoved() const { return mRemoved; }
    bool isVisible() const { return mVisible; }
    bool isAvatarKnown() const;
    bool isInformationKnown() const;

    Info info() const;

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

QDataStream& operator<<(QDataStream &stream, const Tp::Presence &presence);
QDataStream& operator<<(QDataStream &stream, const Tp::ContactInfoField &field);
QDataStream& operator<<(QDataStream &stream, const CDTpContact::Info &info);

QDataStream& operator>>(QDataStream &stream, Tp::Presence &presence);
QDataStream& operator>>(QDataStream &stream, Tp::ContactInfoField &field);
QDataStream& operator>>(QDataStream &stream, CDTpContact::Info &info);

#endif // CDTPCONTACT_H
