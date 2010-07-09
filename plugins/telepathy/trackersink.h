/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact:  Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#ifndef TRACKERSINK_H
#define TRACKERSINK_H

#include <QtTracker/Tracker>
#include <QtTracker/QLive>
#include <QtTracker/ontologies/nco.h>
#include <tpcontact.h>

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

    void saveToTracker(const QString&, const QString&, const QString&, const QString&, const QString&, const QString&, Tp::ContactCapabilities*);

    void takeAllOffline(const QString& path);

    void getIMContacts(const QString&);

signals:
    /*! \brief Signal which is emmited when a new avatar is added to the storage
      */
    void  avatarUpdated();

private Q_SLOTS:
   void onAvatarUpdated(const QString&, const QString&, const QString&);
   void onSimplePresenceChanged(TpContact* contact, uint uniqueId);
   void onCapabilities(TpContact*);
   void onFeaturesReady(TpContact*);
   void onChange(uint uniqueId, TpContact::ChangeType type);
   void onModelUpdate();

private:
    void connectOnSignals(TpContactPtr contact);


private:
    TrackerSink();
    /**
     * Return a service pointer: if transaction is on - from transaction.
     * If transactions is committed - returns tracker.
     */
    RDFServicePtr service();
    void commitTrackerTransaction();
    void initiateTrackerTransaction();
    TpContact* find(uint id);

    bool isValidTpContact(const TpContact *tpContact);
    class Private;
    Private * const d;
    friend class ut_trackersink;
};

#endif // TRACKERSINK_H
