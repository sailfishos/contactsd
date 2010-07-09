/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact:  Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */


#ifndef TELEPATHYCONTACT_H
#define TELEPATHYCONTACT_H

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

class TelepathyContact : public QObject
{
    Q_OBJECT
public:
    /*! \brief Contstucts a Telepathy Contact which is
      * writable using an existing Tp::Contact
      * \param contact which is meant to be wrapped
      * \param id the group id which this contact should belong to
      */
    explicit TelepathyContact(Tp::ContactPtr contact, int groupID = -1);
    explicit TelepathyContact(QObject * parent = 0);
    virtual ~TelepathyContact();

    void setId(const QString& Id);
    QString id() const;

    void setAlias(const QString& Alias);
    QString alias() const;

    void setType(int atype);
    int type() const;

    void setMessage(const QString&);
    QString message();

    void setPresenceStatus(const QString&);
    QString presenceStatus() const;

    void setAvatarToken(const QString&, const QString&);
    /*!
     * This looks like avatar token and it is used to track for which contact
     * avatar is being set
     */
    QString avatar() const;
    /*!
     * avatar file path. If avatar image has not yet been loaded, returns empty string
     */
    QString avatarFilePath() const;


    void requestCapabilities();
    void requestStorageId();

    void setAccountPath(const QString&);
    QString accountPath() const;

    Tp::ContactPtr contact();

    Tp::ContactCapabilities* contactCapabilities() const;
    uint storageId() const;
    unsigned int uniqueId() const;

    Q_DECL_DEPRECATED bool hasCapabilities();
    Q_DECL_DEPRECATED bool hasContactCapabilities();
    Q_DECL_DEPRECATED bool hasStorageId();
    Q_DECL_DEPRECATED Tp::ContactCapabilityList capabilities() const;

public Q_SLOTS:
    void onSimplePresenceChanged(const QString & status , uint stat, const QString & message);
    void onAvatarRetrieved(uint, const QString&, const QByteArray&, const QString&);
    void onAvatarUpdated(uint, const QString&);
    Q_DECL_DEPRECATED void onCapFinished(QDBusPendingCallWatcher * self);
    void onExtCapFinished(Tp::ContactCapabilities* caps);
    void onFeaturesReady(Tp::PendingOperation *);

Q_SIGNALS:
    void simplePresenceChanged(TelepathyContact*, const QString & status , uint stat, const QString & message, int GroupID);
    void avatarReady();
    void avatarReady(const QString& id, const QString& token, const QString& mime);
    Q_DECL_DEPRECATED void capabilities(TelepathyContact*, Tp::ContactCapabilityList);
    void featuresReady(TelepathyContact*);

private:
    Tp::ContactPtr mContact;
    int mGroupID;
    QString mId, mMessage, mAlias;
    int mType;
    QString mAvatar;
    QString  mStatusString;
    Tp::ConnectionPtr mConnection;
    QSharedPointer<QDBusPendingCallWatcher> mWatcher;
    QString mAccountPath;
    Tp::ContactCapabilityList mCaps;
    Tp::ContactCapabilities* mCapabilities;
    bool mHasStorageId;
    uint mStorageId;

    // This _uniqueId is used for tests. test set it so that it is not fetched fro
    // somewhere else
    unsigned int _uniqueId;

    friend class ut_trackersink;
};

typedef QList<QSharedPointer<TelepathyContact> > ContactList;
typedef QSharedPointer<TelepathyContact> TelepathyContactPtr;

#endif // TELEPATHYCONTACT_H
