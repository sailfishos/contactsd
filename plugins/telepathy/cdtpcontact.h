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

#include <TelepathyQt4/ContactCapabilities>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/Connection>

#include <QObject>

/*
 * Wraps a telepathy contact and also supports caching of
 * avatars and the contact itself.
 */
class CDTpContact : public QObject
{
    Q_OBJECT

public:
    typedef enum {
        SIMPLE_PRESENCE = 0,
        AVATAR_TOKEN,
        FEATURES,
        CAPABILITIES
    } ChangeType;

    explicit CDTpContact(Tp::ContactPtr contact, QObject *parent = 0);
    explicit CDTpContact(QObject *parent = 0);
    virtual ~CDTpContact();

    QSharedPointer<const Tp::Contact> contact() const;

    // following Tp::Contact wrappers are virtual for stubbing purposes
    virtual QString id() const;
    virtual QString alias() const;
    virtual unsigned int presenceType() const;
    virtual QString presenceMessage() const;

    // capabilities section
    virtual bool supportsTextChats() const;
    virtual bool supportsMediaCalls() const;
    virtual bool supportsAudioCalls() const;
    virtual bool supportsVideoCalls(bool withAudio = true) const;
    virtual bool supportsUpgradingCalls() const;
    Tp::ContactCapabilities *capabilities() const;

    QString accountPath() const;
    void setAccountPath(const QString &accountPath);

    QString avatar() const;
    QString avatarMime() const;

    unsigned int uniqueId() const;
    static unsigned int buildUniqueId(const QString &accountPath,
            const QString &imId);

    QUrl imAddress() const;
    static QUrl buildImAddress(const QString &accountPath, const QString &imId);

    bool isReady() const;

Q_SIGNALS:
    void ready(CDTpContact *);
    void change(uint uniqueId, CDTpContact::ChangeType type);

private Q_SLOTS:
    void onSimplePresenceChanged(const QString &status , uint stat, const QString &message);
    void onAvatarRetrieved(uint, const QString &, const QByteArray &, const QString &);
    void onAvatarUpdated(uint, const QString &);
    void onExtCapFinished(Tp::ContactCapabilities *caps);
    void onFeaturesReady(Tp::PendingOperation *op);

private:
    friend class ut_trackersink;

    void reqestFeatures(Tp::ContactPtr);
    void requestAvatar(Tp::ContactPtr);
    void requestCapabilities();

    class CDTpContactPrivate;
    CDTpContactPrivate* const d;
};

typedef QList<QSharedPointer<CDTpContact> > CDTpContactList;
typedef QSharedPointer<CDTpContact> CDTpContactPtr;

#endif // CDTPCONTACT_H
