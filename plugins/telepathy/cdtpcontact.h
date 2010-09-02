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

/** A class which wrapps a telepathy contact
  and also supports caching of avatars and
  the contact it self. you can create instance
  of this and also read and write from it
  **/

class TpContact : public QObject
{
    Q_OBJECT
public:
    typedef enum { SIMPLE_PRESENCE = 0
                   , AVATAR_TOKEN
                   , FEATURES
                   , CAPABILITIES
                 } ChangeType;

    explicit TpContact(Tp::ContactPtr contact, QObject* parent = 0);
    explicit TpContact(QObject * parent = 0);
    virtual ~TpContact();

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
    void setAccountPath(const QString&);

    QString avatar() const;
    QString avatarMime() const;

    unsigned int uniqueId() const;
    static unsigned int buildUniqueId(const QString& accountPath, const QString& imId);

    QUrl imAddress() const;
    static QUrl buildImAddress(const QString& accountPath, const QString& imId);

    bool isReady() const;

Q_SIGNALS:
    void ready(TpContact*);
    void change(uint uniqueId, TpContact::ChangeType type);

private Q_SLOTS:
    void onSimplePresenceChanged(const QString & status , uint stat, const QString & message);
    void onAvatarRetrieved(uint, const QString&, const QByteArray&, const QString&);
    void onAvatarUpdated(uint, const QString&);
    void onExtCapFinished(Tp::ContactCapabilities* caps);
    void onFeaturesReady(Tp::PendingOperation*);

private:
    void reqestFeatures(Tp::ContactPtr);
    void requestAvatar(Tp::ContactPtr);
    void requestCapabilities();

    class TpContactPrivate;
    TpContactPrivate* const d;

    friend class ut_trackersink;
};

typedef QList<QSharedPointer<TpContact> > TpContactList;
typedef QSharedPointer<TpContact> TpContactPtr;

#endif // CDTPCONTACT_H
