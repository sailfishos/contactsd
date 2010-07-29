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

#include "trackersink.h"

// contactsd
#include <contactphotocopy.h>
// QtTracker
#include <QtTracker/Tracker>
#include <QtTracker/QLive>
#include <QtTracker/ontologies/nco.h>


typedef QHash<uint, QSharedPointer<TpContact> > ContactMapHash;

class TrackerSink::Private
{
public:
    Private():transaction_(0) {}
    ~Private() {
    }
    ContactMapHash contactMap;
    RDFTransactionPtr transaction_;
    QHash<uint, uint> presenceHash; // maps tpCId hash to presence message hash
    LiveNodes livenode;

};

TrackerSink* TrackerSink::instance()
{
    static TrackerSink instance_;
    return &instance_;
}

TrackerSink::TrackerSink(): d(new Private)
{
}

TrackerSink::~TrackerSink()
{
    delete d;
}

void TrackerSink::getIMContacts(const QString& /* contact_iri */)
{
    RDFSelect select;

    RDFVariable contact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imaddress = contact.property<nco::hasIMAddress>();

    select.addColumn("distinct", imaddress.property<nco::imID>());
    select.addColumn("contactId", contact.property<nco::contactLocalUID> ());

    d->livenode = ::tracker()->modelQuery(select);

    connect(d->livenode.model(), SIGNAL(modelUpdated()), this, SLOT(onModelUpdate()));

}

void TrackerSink::onModelUpdate()
{
    QStringList idList;

    for (int i = 0 ; i < d->livenode->rowCount() ; i ++) {
        const QString imAddress = d->livenode->index(i, 0).data().toString();
        const QString imLocalId = d->livenode->index(i, 1).data().toString();

        bool ok;
        const uint localId = imLocalId.toUInt(&ok, 0);

        ContactMapHash::const_iterator it = d->contactMap.find(localId);
        if (it == d->contactMap.end()) {
            qWarning() << "cannot find TpContact for" << imAddress << localId;
            continue;
        }

        const QSharedPointer<const TpContact> contact = it.value();
        qDebug() << imAddress << imLocalId << ":" << contact;

        const QSharedPointer<const Tp::Contact> tcontact = contact->contact();
        saveToTracker(imLocalId, tcontact->id(),
                      tcontact->alias(),
                      toTrackerStatus(tcontact->presenceType()),
                      tcontact->presenceMessage(),
                      contact->accountPath(),
                      tcontact->capabilities());
    }


    if (d->livenode->rowCount() <= 0) {
        foreach (const QSharedPointer<TpContact>& contact, d->contactMap.values()) {
            if (contact.isNull()) {
                continue;
            }

            const QSharedPointer<const Tp::Contact> tcontact = contact->contact();
            if(!tcontact)
                continue;

            const QString tpCId = contact->accountPath() + "!" + tcontact->id();
            const QString id(QString::number(qHash(tpCId)));

            saveToTracker(id, tcontact->id(),
                          tcontact->alias(),
                          toTrackerStatus(tcontact->presenceType()),
                          tcontact->presenceMessage(),
                          contact->accountPath(),
                          tcontact->capabilities());

        }
    }

}

void TrackerSink::connectOnSignals(TpContactPtr contact)
{
    connect(contact.data(), SIGNAL(change(uint, TpContact::ChangeType)),
            this, SLOT(onChange(uint, TpContact::ChangeType)));
}

TpContact* TrackerSink::find(uint id)
{
    if (d->contactMap.keys().contains(id) ) {
        return d->contactMap[id].data();
    }

    return 0;
}

void TrackerSink::onChange(uint uniqueId, TpContact::ChangeType type)
{
    TpContact* contact = find(uniqueId);
    if (!contact) {
        return;
    }

    switch (type) {
    case TpContact::AVATAR_TOKEN:
        onAvatarUpdated(contact->contact()->id(), contact->avatar(), contact->avatarMime());
        break;
    case TpContact::SIMPLE_PRESENCE:
        onSimplePresenceChanged(contact, uniqueId);
        break;
    case TpContact::CAPABILITIES:
        onCapabilities(contact);
        break;
    case TpContact::FEATURES:
        qDebug() << Q_FUNC_INFO;
        break;
    }
}
void TrackerSink::onFeaturesReady(TpContact* tpContact)
{
    foreach (TpContactPtr contact, d->contactMap) {
        if (contact.data() == tpContact) {
            sinkToStorage(contact);
        }
    }
}

static QUrl buildContactIri(const QString& uniqueIdStr)
{
  return QUrl("contact:" + uniqueIdStr);
}

static QUrl buildContactIri(unsigned int uniqueId)
{
  return buildContactIri(QString::number(uniqueId));
}

void TrackerSink::saveToTracker(const QString& uri, const QString& imId, const QString& nick, const
        QUrl& status, const QString& msg, const QString& accountpath, Tp::ContactCapabilities * contactcaps)
{
    const RDFVariable contact(buildContactIri(uri));

    const QString id(QString::number(TpContact::buildUniqueId(accountpath, imId)));
    const RDFVariable imAddress(TpContact::buildImAddress(accountpath, imId));
    const RDFVariable imAccount(QUrl("telepathy:" + accountpath));

    RDFUpdate addressUpdate;

    addressUpdate.addDeletion(imAddress, nco::imNickname::iri());
    addressUpdate.addDeletion(imAddress, nco::imPresence::iri());
    addressUpdate.addDeletion(imAddress, nco::imStatusMessage::iri());

    addressUpdate.addInsertion(RDFStatementList() <<
                               RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri()) <<
                               RDFStatement(imAddress, nco::imNickname::iri(), LiteralValue(nick)) <<
                               RDFStatement(imAddress, nco::imStatusMessage::iri(), LiteralValue((msg))) <<
                               RDFStatement(imAddress, nco::imPresence::iri(), status) <<
                               RDFStatement(imAddress, nco::imID::iri(), LiteralValue(imId)) );

    addressUpdate.addInsertion(RDFStatementList() <<
                               RDFStatement(contact, rdf::type::iri(), nco::PersonContact::iri()) <<
                               RDFStatement(contact, nco::hasIMAddress::iri(), imAddress) <<
                               RDFStatement(contact, nco::contactLocalUID::iri(), LiteralValue(id)) );

    addressUpdate.addInsertion(RDFStatementList() <<
                               RDFStatement(imAccount, rdf::type::iri(), nco::IMAccount::iri()) <<
                               RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress));

    addressUpdate.addDeletion(imAddress, nco::imCapability::iri());



    if (contactcaps->supportsMediaCalls() || contactcaps->supportsAudioCalls())  {
        addressUpdate.addInsertion( RDFStatementList() <<
                                    RDFStatement(imAddress, nco::imCapability::iri(),
                                                 nco::im_capability_audio_calls::iri()));

    }

    if (contactcaps->supportsTextChats() ) {
        addressUpdate.addInsertion( RDFStatementList() <<
                                    RDFStatement(imAddress, nco::imCapability::iri(),
                                                 nco::im_capability_text_chat::iri()));
    }

    service()->executeQuery(addressUpdate);

}

void TrackerSink::sinkToStorage(const QSharedPointer<TpContact>& obj)
{
    const unsigned int uniqueId = obj->uniqueId();
    if (!find(uniqueId) ) {
        connectOnSignals(obj);
        d->contactMap[uniqueId] = obj;
    }


    if (!obj->isReady()) {
        connect(obj.data(), SIGNAL(ready(TpContact*)),
                this, SLOT(onFeaturesReady(TpContact*)));
        return;
    }

    const QSharedPointer<const Tp::Contact> tcontact = obj->contact();

    qDebug() << Q_FUNC_INFO <<
        " \n{Contact Id :" << tcontact->id() <<
        " }\n{Alias : " <<  tcontact->alias() <<
        " }\n{Status : " << tcontact->presenceType() <<
        " }\n{Message : " << tcontact->presenceMessage() <<
        " }\n{AccountPath : " << obj->accountPath();

    const QString id(QString::number(uniqueId));

    saveToTracker(id, tcontact->id(),
                      tcontact->alias(),
                      toTrackerStatus(tcontact->presenceType()),
                      tcontact->presenceMessage(),
                      obj->accountPath(),
                      tcontact->capabilities());

}

void TrackerSink::onCapabilities(TpContact* obj)
{
    if (!isValidTpContact(obj)) {
        qDebug() << Q_FUNC_INFO << "Invalid Telepathy Contact";
        return;
    }

    const RDFVariable imAddress(obj->imAddress());

    RDFUpdate addressUpdate;

    addressUpdate.addDeletion(imAddress, nco::imCapability::iri());

    const Tp::ContactCapabilities *capabilities = obj->contact()->capabilities();

    if (capabilities && (capabilities->supportsMediaCalls() ||
            capabilities->supportsAudioCalls()) ) {


        //TODO: Move this to the constructor, so it's only called once?
        // This liveNode() call actually sets this RDF triple:
        //   nco::im_capability_audio_calls a rdfs:Resource
        // though the libqtttracker maintainer agrees that it's bad API.
        Live<nco::IMCapability> cap =
            service()->liveNode(nco::im_capability_audio_calls::iri());

        addressUpdate.addInsertion( RDFStatementList() <<
                                    RDFStatement(imAddress, nco::imCapability::iri(),
                                                 nco::im_capability_audio_calls::iri()));

    }

    service()->executeQuery(addressUpdate);

    this->commitTrackerTransaction();
}


void TrackerSink::onSimplePresenceChanged(TpContact* obj, uint uniqueId)
{
    qDebug() << Q_FUNC_INFO;
    if (!isValidTpContact(obj)) {
        qDebug() << Q_FUNC_INFO << "Invalid Telepathy Contact";
        return;
    }

    const RDFVariable contact(buildContactIri(uniqueId));
    const RDFVariable imAddress(obj->imAddress());

    RDFUpdate addressUpdate;

    addressUpdate.addDeletion(imAddress, nco::imPresence::iri());
    addressUpdate.addDeletion(imAddress, nco::imStatusMessage::iri());
    addressUpdate.addDeletion(imAddress, nie::contentLastModified::iri());
    addressUpdate.addDeletion(contact, nie::contentLastModified::iri());

    const QSharedPointer<const Tp::Contact> tcontact = obj->contact();
    const QDateTime datetime = QDateTime::currentDateTime();

    qDebug() << Q_FUNC_INFO << toTrackerStatus(tcontact->presenceType());

    RDFStatementList insertions;
    insertions << RDFStatement(imAddress, nco::imStatusMessage::iri(),
            LiteralValue(tcontact->presenceMessage()));
    insertions << RDFStatement(imAddress, nco::imPresence::iri(), toTrackerStatus(tcontact->presenceType()));
    addressUpdate.addInsertion(insertions);
    addressUpdate.addInsertion(contact, nie::contentLastModified::iri(), RDFVariable(datetime));

    service()->executeQuery(addressUpdate);

    this->commitTrackerTransaction();

}

QList<QSharedPointer<TpContact> > TrackerSink::getFromStorage()
{
    return  d->contactMap.values();
}

bool TrackerSink::contains(const QString& aId)
{
    if (aId.isEmpty()) {
        return false;
    }

    RDFVariable rdfContact = RDFVariable::fromType<nco::Contact>();
    rdfContact.property<nco::hasIMAccount>().property<nco::imID>() = LiteralValue(aId);

    RDFSelect query;
    query.addColumn(rdfContact);
    LiveNodes ncoContacts = ::tracker()->modelQuery(query);
    if (ncoContacts->rowCount() > 0) {
        return true;
    }

    return false;

}

bool TrackerSink::compareAvatar(const QString& token)
{
    Q_UNUSED(token)
    return false;
}


void TrackerSink::saveAvatarToken(const QString& id, const QString& token, const QString& mime)
{
    const QString avatarPath = ContactPhotoCopy::avatarDir()+"/telepathy_cache"+token+'.'+ mime;

    foreach (const QSharedPointer<const TpContact>& c, d->contactMap) {
        const QSharedPointer<const Tp::Contact> tcontact = c->contact();
        if (tcontact && id == tcontact->id() ) {
            using namespace SopranoLive;
            if (!isValidTpContact(c.data())) {
                continue;
            }

            const QUrl tpUrl = c->imAddress();
            Live<nco::IMAddress> address = service()->liveNode(tpUrl);

            //TODO: Can this be moved to later, where it is first used?
            // It has not been moved yet, because some
            // of these liveNode() calls actually set RDF triples,
            // though the libqtttracker maintainer agrees that it's bad API.
            Live<nie::InformationElement> info = service()->liveNode(tpUrl);

            const QString uniqueIdStr = QString::number(c->uniqueId());
            Live<nco::PersonContact> photoAccount = service()->liveNode(buildContactIri(uniqueIdStr));

            // set both properties for a transition period
            // TODO: Set just one when it has been decided:
            photoAccount->setContactLocalUID(uniqueIdStr);
            photoAccount->setContactUID(uniqueIdStr);

            const QDateTime datetime = QDateTime::currentDateTime();
            photoAccount->setContentCreated(datetime);
            info->setContentLastModified(datetime);

            //FIXME:
            //To be removed once tracker plugin reads imaddress photo url's
            Live<nie::DataObject> fileurl = ::tracker()->liveNode(QUrl(avatarPath));
            photoAccount->setPhoto(fileurl);
            address->addImAvatar(fileurl);

            break;
        }
    }
    this->commitTrackerTransaction();
}

void TrackerSink::onAvatarUpdated(const QString& id, const QString& token, const QString& mime)
{
    if (!compareAvatar(token))
        saveAvatarToken(id, token, mime);
}

//TODO: This seems to be useless:
bool TrackerSink::isValidTpContact(const TpContact *tpContact) {
    if (tpContact != 0) {
        return true;
    }

    return false;
}

RDFServicePtr TrackerSink::service()
{
    if (d->transaction_)
    {
        // if transaction was obtained, grab the service from inside it and use it
        return d->transaction_->service();
    }
    else
    {
        // otherwise, use tracker directly, with no transactions.
        return ::tracker();
    }
}

void TrackerSink::commitTrackerTransaction() {
    if ( d->transaction_)
    {
        d->transaction_->commit();
        d->transaction_ = RDFTransactionPtr(0); // that's it, transaction lives until commit
    }
}

void TrackerSink::initiateTrackerTransaction()
{
    if ( !d->transaction_ )
        d->transaction_ = ::tracker()->createTransaction();
}


void TrackerSink::clearContacts(const QString& path)
{
    const QList<uint> list (d->contactMap.keys());
    
    foreach (uint index, list) {
        QSharedPointer<TpContact> contact = d->contactMap[index];
        
        if (!contact) {
            continue;
        }
        
        if (contact->accountPath() != path) {
            continue;
        }
        
        qDebug() << Q_FUNC_INFO << "Removing Contact";

        d->contactMap.remove(index);
    }
}

/* When account goes offline make all contacts as Unknown
   this is a specification requirement
   */

void TrackerSink::takeAllOffline(const QString& path)
{

    initiateTrackerTransaction();
    RDFUpdate addressUpdate;
    foreach (TpContactPtr obj, getFromStorage()) {
        if (obj->accountPath() != path) {
            continue;
        }

        const RDFVariable contact(buildContactIri(obj->uniqueId()));
        const RDFVariable imAddress(obj->imAddress());

        addressUpdate.addDeletion(imAddress, nco::imPresence::iri());
        addressUpdate.addDeletion(imAddress, nco::imStatusMessage::iri());
        addressUpdate.addDeletion(contact, nie::contentLastModified::iri());

        const QLatin1String status("unknown");
        addressUpdate.addInsertion(RDFStatementList() <<
                                   RDFStatement(imAddress, nco::imStatusMessage::iri(),
                                                LiteralValue("")) <<
                                   RDFStatement(imAddress, nco::imPresence::iri(),
                                                toTrackerStatus(status)));

        addressUpdate.addInsertion(contact, nie::contentLastModified::iri(),
                                   RDFVariable(QDateTime::currentDateTime()));
    }

    service()->executeQuery(addressUpdate);
    this->commitTrackerTransaction();
}

const QUrl & TrackerSink::toTrackerStatus(const uint stat)
{
   switch (stat) {
       case Tp::ConnectionPresenceTypeUnset: return nco::presence_status_unknown::iri();
       case Tp::ConnectionPresenceTypeOffline: return nco::presence_status_offline::iri();
       case Tp::ConnectionPresenceTypeAvailable: return nco::presence_status_available::iri();
       case Tp::ConnectionPresenceTypeAway: return nco::presence_status_away::iri();
       case Tp::ConnectionPresenceTypeExtendedAway: return
                                                    nco::presence_status_extended_away::iri();
       case Tp::ConnectionPresenceTypeHidden: return nco::presence_status_hidden::iri();
       case Tp::ConnectionPresenceTypeBusy: return nco::presence_status_busy::iri();
       case Tp::ConnectionPresenceTypeUnknown: return nco::presence_status_unknown::iri();
       case Tp::ConnectionPresenceTypeError: return nco::presence_status_error::iri();
       default: qWarning() << Q_FUNC_INFO << "Presence Status Unknown";
   }

   return nco::presence_status_error::iri();
}

const QUrl & TrackerSink::toTrackerStatus(const QString& status)
{
    static QHash<QString, QUrl> mapping;

    if (mapping.isEmpty()) {
        mapping.insert("offline", nco::presence_status_offline::iri());
        mapping.insert("available", nco::presence_status_available::iri());
        mapping.insert("away", nco::presence_status_away::iri());
        mapping.insert("xa", nco::presence_status_extended_away::iri());
        mapping.insert("dnd", nco::presence_status_busy::iri());
        mapping.insert("unknown", nco::presence_status_unknown::iri());
        mapping.insert("hidden", nco::presence_status_hidden::iri());
        mapping.insert("busy", nco::presence_status_busy::iri());
    }

    QHash<QString, QUrl>::const_iterator i(mapping.find(status));

    if (i != mapping.end()) {
        return *i;
    }

    return nco::presence_status_error::iri();
}
