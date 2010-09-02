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

#ifndef TRACKERSINK_H
#define TRACKERSINK_H

#include "cdtpcontact.h"

namespace SopranoLive {
    class RDFService;
    typedef QSharedPointer <RDFService > RDFServicePtr;
}
using namespace SopranoLive;

/**
  * \class TrackerSink
  * This class saves telepathy contacts into tracker and keeps them in sync
  */
class TrackerSink : public QObject
{
    Q_OBJECT
public:

    static TrackerSink * instance();
    static const QUrl & toTrackerStatus(const QString& tpStatus);

    virtual ~TrackerSink();

     /*! \brief saves a telepathy contact into the database
      * \param contact The telepathy contact you wish to store
      */
    void sinkToStorage(const QSharedPointer<TpContact>& contact);
    /*! \brief retuns the contacts saved in the database
      * \return  the list of contacts ContactList
      */

    TpContactList getFromStorage();
    /*! \brief see if a give contact is already in the database
      * \param id The id of the contact ex: you@gmail.com
      * \return true if contact is in the database false otherwise
      */
    bool contains(const QString& id);

    bool compareAvatar(const QString& token);

    QSharedPointer<TpContact> getContactFromCache(const QString&);

    void saveAvatarToken(const QString& id, const QString& token, const QString& mime);

    // TODO QString contactLocalId to QContactLocalId. Why it is QString?
    void saveToTracker(const QString& contactLocalId, const TpContact *tpContact);

    void takeAllOffline(const QString& path);
    void clearContacts(const QString& path);
    void deleteContacts(const QString& path);
    void deleteContact(const QSharedPointer<TpContact>& contact);
    

    void getIMContacts(const QString&);

    // transaction used for batch saving.
    void initiateTrackerTransaction();
    void commitTrackerTransaction();

    /*!
     * TODO QHash lookup - if there is already created nco:PersonContacts in tracker
     * for given \a tpContact.
     *
     * \a existing - false for not existing contacts
     */
    unsigned int contactLocalUID(const TpContact* const tpContact, bool *existing = 0) const;

Q_SIGNALS:
    /*! \brief Signal which is emmited when a new avatar is added to the storage
      */
    void  avatarUpdated();

private Q_SLOTS:
   void onAvatarUpdated(const QString&, const QString&, const QString&);
   void onSimplePresenceChanged(TpContact* contact);
   void onCapabilities(TpContact*);
   void onFeaturesReady(TpContact*);
   void onChange(uint uniqueId, TpContact::ChangeType type);
   void onModelUpdate();
   void onDeleteModelReady();

private:
    void connectOnSignals(TpContactPtr contact);
    const QUrl& toTrackerStatus(const uint stat);


private:
    TrackerSink();
    /**
     * Return a service pointer: if transaction is on - from transaction.
     * If transactions is committed - returns tracker.
     */
    RDFServicePtr service();
    TpContact* find(uint id);

    bool isValidTpContact(const TpContact *tpContact);
    class Private;
    Private * const d;
    friend class ut_trackersink;
};

#endif // TRACKERSINK_H
