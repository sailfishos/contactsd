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

#include <TelepathyQt4/AvatarData>
#include <TelepathyQt4/ContactCapabilities>

#include "cdtpaccount.h"
#include "cdtpcontact.h"
#include "debug.h"

using namespace Contactsd;

///////////////////////////////////////////////////////////////////////////////

class CDTpContact::InfoData : public QSharedData
{
public:
    InfoData();

    QString alias;
    Tp::Presence presence;
    int capabilities;
    QString avatarPath;
    QString largeAvatarPath;
    QString squareAvatarPath;
    Tp::Contact::PresenceState subscriptionState;
    Tp::Contact::PresenceState publishState;
    Tp::ContactInfoFieldList infoFields;
    bool isSubscriptionStateKnown : 1;
    bool isPublishStateKnown : 1;
    bool isContactInfoKnown : 1;
    bool isVisible : 1;
};

CDTpContact::InfoData::InfoData()
    : isSubscriptionStateKnown(false)
    , isPublishStateKnown(false)
    , isContactInfoKnown(false)
    , isVisible(false)
{}

///////////////////////////////////////////////////////////////////////////////

static CDTpContact::Info::Capabilities makeInfoCaps(const Tp::CapabilitiesBase &capabilities)
{
    CDTpContact::Info::Capabilities caps = 0;

    if (capabilities.textChats()) {
        caps |= CDTpContact::Info::TextChats;
    }
    if (capabilities.streamedMediaCalls()) {
        caps |= CDTpContact::Info::StreamedMediaCalls;
    }
    if (capabilities.streamedMediaAudioCalls()) {
        caps |= CDTpContact::Info::StreamedMediaAudioCalls;
    }
    if (capabilities.streamedMediaVideoCalls()) {
        caps |= CDTpContact::Info::StreamedMediaAudioVideoCalls;
    }
    if (capabilities.upgradingStreamedMediaCalls()) {
        caps |= CDTpContact::Info::UpgradingStreamMediaCalls;
    }
    if (capabilities.fileTransfers()) {
        caps |= CDTpContact::Info::FileTransfers;
    }

    return caps;
}

CDTpContact::Info::Info()
    :d(new CDTpContact::InfoData)
{
}

CDTpContact::Info::Info(const CDTpContact *contact)
    : d(new CDTpContact::InfoData)
{
    const Tp::ContactPtr c = contact->contact();

    d->alias = c->alias();
    d->presence = c->presence();
    d->capabilities = makeInfoCaps(c->capabilities());
    d->avatarPath = c->avatarData().fileName;
    d->subscriptionState = c->subscriptionState();
    d->publishState = c->publishState();
    d->infoFields = c->infoFields().allFields();
    d->isSubscriptionStateKnown = c->isSubscriptionStateKnown();
    d->isPublishStateKnown = c->isPublishStateKnown();
    d->isContactInfoKnown = c->isContactInfoKnown();
    d->isVisible = contact->isVisible();
}

CDTpContact::Info::Info(const CDTpContact::Info &other)
    : d(other.d)
{
}

CDTpContact::Info& CDTpContact::Info::operator=(const CDTpContact::Info &other)
{
    return (d = other.d, *this);
}

CDTpContact::Info::~Info()
{
}

CDTpContact::Changes CDTpContact::Info::diff(const CDTpContact::Info &other) const
{
    Changes changes = 0;

    if (d->alias != other.d->alias)
        changes |= CDTpContact::Alias;

    // We only compare the relevant fields (status is not saved in Tracker, and isValid is irrelevant)
    if (d->presence.type() != other.d->presence.type()
     || d->presence.statusMessage() != other.d->presence.statusMessage())
        changes |= CDTpContact::Presence;

    if (d->capabilities != other.d->capabilities)
        changes |= CDTpContact::Capabilities;

    if (d->avatarPath != other.d->avatarPath)
        changes |= CDTpContact::DefaultAvatar;

    if (d->largeAvatarPath != other.d->largeAvatarPath)
        changes |= CDTpContact::LargeAvatar;

    if (d->squareAvatarPath != other.d->squareAvatarPath)
        changes |= CDTpContact::SquareAvatar;

    if (d->isSubscriptionStateKnown != other.d->isSubscriptionStateKnown
     || d->isPublishStateKnown != other.d->isPublishStateKnown
     || d->subscriptionState != other.d->subscriptionState
     || d->publishState != other.d->publishState)
        changes |= CDTpContact::Authorization;

    if (other.d->isContactInfoKnown
     && d->infoFields != other.d->infoFields)
        changes |= CDTpContact::Information;

    if (d->isVisible != other.d->isVisible)
        changes |= CDTpContact::Visibility;

    return changes;
}

///////////////////////////////////////////////////////////////////////////////

CDTpContact::CDTpContact(Tp::ContactPtr contact, CDTpAccount *accountWrapper)
    : QObject(),
      mContact(contact),
      mAccountWrapper(accountWrapper),
      mRemoved(false),
      mQueuedChanges(0)
{
    mQueuedChangesTimer.setInterval(0);
    mQueuedChangesTimer.setSingleShot(true);
    connect(&mQueuedChangesTimer, SIGNAL(timeout()), SLOT(onQueuedChangesTimeout()));

    updateVisibility();

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
            SIGNAL(subscriptionStateChanged(Tp::Contact::PresenceState)),
            SLOT(onContactAuthorizationChanged()));
    connect(contact.data(),
            SIGNAL(publishStateChanged(Tp::Contact::PresenceState, const QString &)),
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

CDTpAccountPtr CDTpContact::accountWrapper() const
{
    return CDTpAccountPtr(mAccountWrapper.data());
}

bool CDTpContact::isAvatarKnown() const
{
    if (!mContact->isAvatarTokenKnown()) {
        return false;
    }

    /* If we have a token but not an avatar filename, that probably means the
     * avatar data is being requested and we'll get an update later. */
    if (!mContact->avatarToken().isEmpty() &&
        mContact->avatarData().fileName.isEmpty()) {
        return false;
    }

    return true;
}

bool CDTpContact::isInformationKnown() const
{
    return mContact->isContactInfoKnown();
}

CDTpContact::Info CDTpContact::info() const
{
    return Info(this);
}

void CDTpContact::setLargeAvatarPath(const QString &path)
{
    mLargeAvatarPath = path;
    emitChanged(LargeAvatar);
}

void CDTpContact::setSquareAvatarPath(const QString &path)
{
    mSquareAvatarPath = path;
    emitChanged(SquareAvatar);
}

void CDTpContact::onContactAliasChanged()
{
    emitChanged(Alias);
}

void CDTpContact::onContactPresenceChanged()
{
    emitChanged(Presence);
}

void CDTpContact::onContactCapabilitiesChanged()
{
    emitChanged(Capabilities);
}

void CDTpContact::onContactAvatarDataChanged()
{
    emitChanged(DefaultAvatar);
}

void CDTpContact::onContactAuthorizationChanged()
{
    emitChanged(Authorization);
}

void CDTpContact::onContactInfoChanged()
{
    emitChanged(Information);
}

void CDTpContact::onBlockStatusChanged()
{
    emitChanged(Blocked);
}

void CDTpContact::emitChanged(CDTpContact::Changes changes)
{
    mQueuedChanges |= changes;

    if (not mQueuedChangesTimer.isActive()) {
        mQueuedChangesTimer.start();
    }
}

void CDTpContact::onQueuedChangesTimeout()
{
    // Check if this change also modified the visibility
    bool wasVisible = mVisible;
    updateVisibility();
    if (mVisible != wasVisible) {
        mQueuedChanges |= Visibility;
    }

    Q_EMIT changed(CDTpContactPtr(this), mQueuedChanges);

    mQueuedChanges = 0;
}

void CDTpContact::updateVisibility()
{
    /* Don't import contacts blocked, removed or incoming auth requests (because
     * user never asked for them). Note that we still import contacts that have
     * publishState==subscribeState==No, because that case happens if we sent an
     * auth request but it got rejected. In the case we received an auth request
     * and we rejected it, with our implementation, the contact is completely
     * removed from the roster so it won't appear here at all (some other IM
     * clients could still keep the contact in the roster with
     * publishState==subscribeState==No, but that's really corner case so we
     * don't care). */
    mVisible = !mRemoved && !mContact->isBlocked() &&
        (mContact->publishState() != Tp::Contact::PresenceStateAsk ||
         mContact->subscriptionState() != Tp::Contact::PresenceStateNo);
}

void CDTpContact::setRemoved(bool value)
{
    mRemoved = value;
    updateVisibility();
}

QDataStream& operator<<(QDataStream &stream, const Tp::Presence &presence)
{
    stream << presence.type();
    stream << presence.status();
    stream << presence.statusMessage();

    return stream;
}

QDataStream& operator<<(QDataStream &stream, const Tp::ContactInfoField &field)
{
    stream << field.fieldName;
    stream << field.parameters;
    stream << field.fieldValue;

    return stream;
}

QDataStream& operator<<(QDataStream &stream, const CDTpContact::Info &info)
{
    stream << info.d->alias;
    stream << info.d->presence;
    stream << info.d->capabilities;
    stream << info.d->avatarPath;
    stream << info.d->largeAvatarPath;
    stream << info.d->squareAvatarPath;
    stream << info.d->isSubscriptionStateKnown;
    stream << uint(info.d->subscriptionState);
    stream << info.d->isPublishStateKnown;
    stream << uint(info.d->publishState);
    stream << info.d->isContactInfoKnown;
    stream << info.d->infoFields;
    stream << info.d->isVisible;

    return stream;
}

QDataStream& operator>>(QDataStream &stream, Tp::Presence &presence)
{
    uint type;
    QString status;
    QString statusMessage;

    stream >> type;
    stream >> status;
    stream >> statusMessage;

    presence.setStatus((Tp::ConnectionPresenceType)type, status, statusMessage);

    return stream;
}

QDataStream& operator>>(QDataStream &stream, Tp::ContactInfoField &field)
{
    stream >> field.fieldName;
    stream >> field.parameters;
    stream >> field.fieldValue;

    return stream;
}

static QDataStream& operator>>(QDataStream &stream, Tp::Contact::PresenceState &presenceState)
{
    uint value;
    stream >> value;
    presenceState = Tp::Contact::PresenceState(value);
    return stream;
}

QDataStream& operator>>(QDataStream &stream, CDTpContact::Info &info)
{
    bool isSubscriptionStateKnown;
    bool isPublishStateKnown;
    bool isContactInfoKnown;
    bool isVisible;

    stream >> info.d->alias;
    stream >> info.d->presence;
    stream >> info.d->capabilities;
    stream >> info.d->avatarPath;
    stream >> info.d->largeAvatarPath;
    stream >> info.d->squareAvatarPath;
    stream >> isSubscriptionStateKnown;
    stream >> info.d->subscriptionState;
    stream >> isPublishStateKnown;
    stream >> info.d->publishState;
    stream >> isContactInfoKnown;
    stream >> info.d->infoFields;
    stream >> isVisible;

    info.d->isSubscriptionStateKnown = isSubscriptionStateKnown;
    info.d->isPublishStateKnown = isPublishStateKnown;
    info.d->isContactInfoKnown = isContactInfoKnown;
    info.d->isVisible = isVisible;

    return stream;
}

