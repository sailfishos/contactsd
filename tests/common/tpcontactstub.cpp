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

#include "tpcontactstub.h"
#include <QtCore>
#include <QImage>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/PendingContacts>
#include <contactphotocopy.h>

const QString TpContactStub::CAPABILITIES_TEXT = QLatin1String("Text");
const QString TpContactStub::CAPABILITIES_MEDIA = QLatin1String("StreamedMedia");

/*!
 * utility method just for usage here
 */
const QString avatarFilePath(QString avatarToken, QString extension)
{
    return ContactPhotoCopy::avatarDir()+"/telepathy_cache"+avatarToken+'.'+extension;
}

TpContactStub::TpContactStub(QObject * parent) : TpContact(parent),
        mContact(0), mPresenceType(0), _uniqueId(0)
{
}

TpContactStub::~TpContactStub()
{
}

void TpContactStub::setId(const QString& Id)
{
       mId = Id;
}

QString TpContactStub::id( ) const
{
    return mId;
}

void TpContactStub::setAlias(const QString& Alias)
{
    mAlias = Alias;
}
QString TpContactStub::alias() const
{
    return mAlias;
}

void TpContactStub::setPresenceMessage(const QString& aMessage)
{
    mPresenceMessage = aMessage;
}

QString TpContactStub::presenceMessage() const
{
    return mPresenceMessage;
}

void TpContactStub::setAvatarToken(const QString& avatar, const QString& mime)
{
    if(mAvatar != avatar) {
        mAvatar = avatar;
    }
}

QString TpContactStub::avatar() const
{
    return mAvatar;
}

QString TpContactStub::avatarFilePath() const
{
    //TODO merge in Siraj's changes about avatar path
    return mAvatar.isEmpty()? QString() : ::avatarFilePath(mAvatar, "jpeg");
}

void TpContactStub::setAccountPath(const QString& paths)
{
    mAccountPath = paths;
}

QString TpContactStub::accountPath() const
{
    return mAccountPath;
}

Tp::ContactPtr TpContactStub::contact() const
{
    return mContact;
}

void TpContactStub::setCapabilitiesStrings(const QStringList &caps)
{
    mCapabilities = caps;
}

unsigned int TpContactStub::presenceType() const
{
    return mPresenceType;
}

void TpContactStub::setPresenceType(const unsigned int type)
{
    mPresenceType = type;
}

bool TpContactStub::supportsTextChats() const
{
    return mCapabilities.contains(CAPABILITIES_TEXT);
}

bool TpContactStub::supportsMediaCalls() const
{
    return mCapabilities.contains(CAPABILITIES_MEDIA);
}

bool TpContactStub::supportsAudioCalls() const
{
    return supportsMediaCalls();
}


