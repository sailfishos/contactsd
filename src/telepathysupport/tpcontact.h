/* * This file is part of contacts *
 * Copyright Â© 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact:  Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */


#ifndef TP_CONTACT_H
#define TP_CONTACT_H

#include <QObject>

#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/ContactCapabilities>
#include <QImage>

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

    //TODO: Is this non-const accessor even used?
    Tp::ContactPtr contact();

    QSharedPointer<const Tp::Contact> contact() const;

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

#endif // TELEPATHYCONTACT_H
