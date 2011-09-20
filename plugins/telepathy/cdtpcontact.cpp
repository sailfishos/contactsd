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

QString CDTpContact::Info::alias() const
{
    return d->alias;
}

void CDTpContact::Info::setAlias(const QString &alias)
{
    d->alias = alias;
}

Tp::Presence CDTpContact::Info::presence() const
{
    return d->presence;
}

void CDTpContact::Info::setPresence(const Tp::Presence &presence)
{
    d->presence = presence;
}

int CDTpContact::Info::capabilities() const
{
    return d->capabilities;
}

void CDTpContact::Info::setCapabilities(int capabilities)
{
    d->capabilities = capabilities;
}

QString CDTpContact::Info::avatarPath() const
{
    return d->avatarPath;
}

void CDTpContact::Info::setAvatarPath(const QString &avatarPath)
{
    d->avatarPath = avatarPath;
}

bool CDTpContact::Info::isSubscriptionStateKnown() const
{
    return d->isSubscriptionStateKnown;
}

void CDTpContact::Info::setSubscriptionStateKnown(bool known)
{
    d->isSubscriptionStateKnown = known;
}

Tp::Contact::PresenceState CDTpContact::Info::subscriptionState() const
{
    return d->subscriptionState;
}

void CDTpContact::Info::setSubscriptionState(Tp::Contact::PresenceState state)
{
    d->subscriptionState = state;
}

bool CDTpContact::Info::isPublishStateKnown() const
{
    return d->isPublishStateKnown;
}

void CDTpContact::Info::setPublishStateKnown(bool known)
{
    d->isPublishStateKnown = known;
}

Tp::Contact::PresenceState CDTpContact::Info::publishState() const
{
    return d->publishState;
}

void CDTpContact::Info::setPublishState(Tp::Contact::PresenceState state)
{
    d->publishState = state;
}

bool CDTpContact::Info::isContactInfoKnown() const
{
    return d->isContactInfoKnown;
}

void CDTpContact::Info::setContactInfoKnown(bool known)
{
    d->isContactInfoKnown = known;
}

Tp::ContactInfoFieldList CDTpContact::Info::infoFields() const
{
    return d->infoFields;
}

void CDTpContact::Info::setInfoFields(const Tp::ContactInfoFieldList &fields)
{
    d->infoFields = fields;
}

bool CDTpContact::Info::isVisible() const
{
    return d->isVisible;
}

void CDTpContact::Info::setVisible(bool visible)
{
    d->isVisible = visible;
}

CDTpContact::Changes CDTpContact::Info::diff(const CDTpContact::Info &other) const
{
    Changes changes = 0;

    if (d->alias != other.alias())
        changes |= CDTpContact::Alias;

    // We only compare the relevant fields (status is not saved in Tracker, and isValid is irrelevant)
    if (d->presence.type() != other.presence().type()
     || d->presence.statusMessage() != other.presence().statusMessage())
        changes |= CDTpContact::Presence;

    if (d->capabilities != other.capabilities())
        changes |= CDTpContact::Capabilities;

    if (d->avatarPath != other.avatarPath())
        changes |= CDTpContact::Avatar;

    if (d->isSubscriptionStateKnown != other.isSubscriptionStateKnown()
     || d->isPublishStateKnown != other.isPublishStateKnown()
     || d->subscriptionState != other.subscriptionState()
     || d->publishState != other.publishState())
        changes |= CDTpContact::Authorization;

    if (other.isContactInfoKnown()
     && d->infoFields != other.infoFields())
        changes |= CDTpContact::Information;

    if (d->isVisible != other.isVisible())
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
    emitChanged(Avatar);
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
    stream << info.alias();
    stream << info.presence();
    stream << info.capabilities();
    stream << info.avatarPath();
    stream << info.isSubscriptionStateKnown();
    stream << (uint)info.subscriptionState();
    stream << info.isPublishStateKnown();
    stream << (uint)info.publishState();
    stream << info.isContactInfoKnown();
    stream << info.infoFields();
    stream << info.isVisible();

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

QDataStream& operator>>(QDataStream &stream, CDTpContact::Info &info)
{
    QString alias;
    stream >> alias;
    info.setAlias(alias);

    Tp::Presence presence;
    stream >> presence;
    info.setPresence(presence);

    int capabilities;
    stream >> capabilities;
    info.setCapabilities(capabilities);

    bool isSubscriptionStateKnown;
    stream >> isSubscriptionStateKnown;
    info.setSubscriptionStateKnown(isSubscriptionStateKnown);

    uint subscriptionState;
    stream >> subscriptionState;
    info.setSubscriptionState((Tp::Contact::PresenceState)subscriptionState);

    bool isPublishStateKnown;
    stream >> isPublishStateKnown;
    info.setPublishStateKnown(isPublishStateKnown);

    uint publishState;
    stream >> publishState;
    info.setPublishState((Tp::Contact::PresenceState)publishState);

    bool isContactInfoKnown;
    stream >> isContactInfoKnown;
    info.setContactInfoKnown(isContactInfoKnown);

    Tp::ContactInfoFieldList fields;
    stream >> fields;
    info.setInfoFields(fields);

    bool isVisible;
    stream >> isVisible;
    info.setVisible(isVisible);

    return stream;
}

