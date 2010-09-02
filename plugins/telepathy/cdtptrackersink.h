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

#ifndef CDTPTRACKERSINK_H
#define CDTPTRACKERSINK_H

#include "cdtpcontact.h"

namespace SopranoLive {
    class RDFService;
    typedef QSharedPointer <RDFService > RDFServicePtr;
}
using namespace SopranoLive;

/**
  * \class CDTpTrackerSink
  * This class saves telepathy contacts into tracker and keeps them in sync
  */
class CDTpTrackerSink : public QObject
{
    Q_OBJECT
public:

    static CDTpTrackerSink * instance();
    static const QUrl & toTrackerStatus(const QString& tpStatus);

    virtual ~CDTpTrackerSink();

     /*! \brief saves a telepathy contact into the database
      * \param contact The telepathy contact you wish to store
      */
    void sinkToStorage(const QSharedPointer<CDTpContact>& contact);
    /*! \brief retuns the contacts saved in the database
      * \return  the list of contacts ContactList
      */

    CDTpContactList getFromStorage();
    /*! \brief see if a give contact is already in the database
      * \param id The id of the contact ex: you@gmail.com
      * \return true if contact is in the database false otherwise
      */
    bool contains(const QString& id);

    bool compareAvatar(const QString& token);

    QSharedPointer<CDTpContact> getContactFromCache(const QString&);

    void saveAvatarToken(const QString& id, const QString& token, const QString& mime);

    // TODO QString contactLocalId to QContactLocalId. Why it is QString?
    void saveToTracker(const QString& contactLocalId, const CDTpContact *tpContact);

    void takeAllOffline(const QString& path);
    void clearContacts(const QString& path);
    void deleteContacts(const QString& path);
    void deleteContact(const QSharedPointer<CDTpContact>& contact);
    

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
    unsigned int contactLocalUID(const CDTpContact* const tpContact, bool *existing = 0) const;

Q_SIGNALS:
    /*! \brief Signal which is emmited when a new avatar is added to the storage
      */
    void  avatarUpdated();

private Q_SLOTS:
   void onAvatarUpdated(const QString&, const QString&, const QString&);
   void onSimplePresenceChanged(CDTpContact* contact);
   void onCapabilities(CDTpContact*);
   void onFeaturesReady(CDTpContact*);
   void onChange(uint uniqueId, CDTpContact::ChangeType type);
   void onModelUpdate();
   void onDeleteModelReady();

private:
    void connectOnSignals(CDTpContactPtr contact);
    const QUrl& toTrackerStatus(const uint stat);


private:
    CDTpTrackerSink();
    /**
     * Return a service pointer: if transaction is on - from transaction.
     * If transactions is committed - returns tracker.
     */
    RDFServicePtr service();
    CDTpContact* find(uint id);

    bool isValidCDTpContact(const CDTpContact *tpContact);
    class Private;
    Private * const d;
    friend class ut_trackersink;
};

#endif // CDTPTRACKERSINK_H
