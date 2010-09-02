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

#include "cdtptrackersink.h"

#include "cdtpcontactphotocopy.h"

#include <QtTracker/ontologies/nco.h>
#include <QtTracker/QLive>
#include <QtTracker/Tracker>

typedef QHash<uint, QSharedPointer<CDTpContact> > ContactMapHash;

class CDTpTrackerSink::Private
{
public:
    Private():transaction_(0) {}
    ~Private() {
    }
    ContactMapHash contactMap;
    RDFTransactionPtr transaction_;
    QHash<uint, uint> presenceHash; // maps tpCId hash to presence message hash
    LiveNodes livenode;
    QString accountToDelete;

};

CDTpTrackerSink* CDTpTrackerSink::instance()
{
    static CDTpTrackerSink instance_;
    return &instance_;
}

CDTpTrackerSink::CDTpTrackerSink(): d(new Private)
{
}

CDTpTrackerSink::~CDTpTrackerSink()
{
    delete d;
}

// TODO dead code
void CDTpTrackerSink::getIMContacts(const QString& /* contact_iri */)
{
    RDFSelect select;

    RDFVariable contact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imaddress = contact.property<nco::hasIMAddress>();

    select.addColumn("distinct", imaddress.property<nco::imID>());
    select.addColumn("contactId", contact.property<nco::contactLocalUID> ());

    d->livenode = ::tracker()->modelQuery(select);

    connect(d->livenode.model(), SIGNAL(modelUpdated()), this, SLOT(onModelUpdate()));

}

// TODO this is dead code - needed in future for merging?
void CDTpTrackerSink::onModelUpdate()
{
    QStringList idList;

    for (int i = 0 ; i < d->livenode->rowCount() ; i ++) {
        const QString imAddress = d->livenode->index(i, 0).data().toString();
        const QString imLocalId = d->livenode->index(i, 1).data().toString();

        bool ok;
        const uint localId = imLocalId.toUInt(&ok, 0);

        ContactMapHash::const_iterator it = d->contactMap.find(localId);
        if (it == d->contactMap.end()) {
            qWarning() << "cannot find CDTpContact for" << imAddress << localId;
            continue;
        }

        const QSharedPointer<const CDTpContact> contact = it.value();
        qDebug() << Q_FUNC_INFO << imAddress << imLocalId << ":" << contact;

        saveToTracker(imLocalId, contact.data());
    }


    // TODO unclear logic in this part of dead code. Document.
    if (d->livenode->rowCount() <= 0) {
        foreach (const QSharedPointer<CDTpContact>& contact, d->contactMap.values()) {
            if (contact.isNull()) {
                continue;
            }

            if(!contact->contact()) {
                qWarning() << Q_FUNC_INFO << "lost contact?" << contact;
                continue;
            }

            const QString tpCId = contact->accountPath() + "!" + contact->id();

            const QString id(QString::number(qHash(tpCId)));

            saveToTracker(id, contact.data());

        }
    }
}

void CDTpTrackerSink::connectOnSignals(CDTpContactPtr contact)
{
    connect(contact.data(), SIGNAL(change(uint, CDTpContact::ChangeType)),
            this, SLOT(onChange(uint, CDTpContact::ChangeType)));
}

CDTpContact* CDTpTrackerSink::find(uint id)
{
    if (d->contactMap.keys().contains(id) ) {
        return d->contactMap[id].data();
    }

    return 0;
}

void CDTpTrackerSink::onChange(uint uniqueId, CDTpContact::ChangeType type)
{

    CDTpContact* contact = find(uniqueId);
    if (!contact) {
        return;
    }

    switch (type) {
    case CDTpContact::AVATAR_TOKEN:
        onAvatarUpdated(contact->id(), contact->avatar(), contact->avatarMime());
        break;
    case CDTpContact::SIMPLE_PRESENCE:
        onSimplePresenceChanged(contact);
        break;
    case CDTpContact::CAPABILITIES:
        onCapabilities(contact);
        break;
    case CDTpContact::FEATURES:
        qDebug() << Q_FUNC_INFO;
        break;
    }
}
void CDTpTrackerSink::onFeaturesReady(CDTpContact* tpContact)
{
    foreach (CDTpContactPtr contact, d->contactMap) {
        if (contact.data() == tpContact) {
            sinkToStorage(contact);
        }
    }
}

static QUrl buildContactIri(const QString& contactLocalIdString)
{
  return QUrl(QString::fromLatin1("contact:") + contactLocalIdString);
}

static QUrl buildContactIri(unsigned int contactLocalId)
{
  return buildContactIri(QString::number(contactLocalId));
}

void CDTpTrackerSink::saveToTracker(const QString& contactLocalId, const CDTpContact *tpContact)
{
    const QString& imId = tpContact->id();
    const QString& nick = tpContact->alias();
    const QUrl& status = toTrackerStatus(tpContact->presenceType());
    const QString& msg = tpContact->presenceMessage();
    const QString& accountpath = tpContact->accountPath();

    const RDFVariable contact(buildContactIri(contactLocalId));

    const QString id(QString::number(CDTpContact::buildUniqueId(accountpath, imId)));
    const RDFVariable imAddress(CDTpContact::buildImAddress(accountpath, imId));
    const RDFVariable imAccount(QUrl(QString::fromLatin1("telepathy:") + accountpath));
    const QDateTime datetime = QDateTime::currentDateTime();

    RDFUpdate addressUpdate;

    addressUpdate.addDeletion(imAddress, nco::imNickname::iri());
    addressUpdate.addDeletion(imAddress, nco::imPresence::iri());
    addressUpdate.addDeletion(imAddress, nco::imStatusMessage::iri());
    addressUpdate.addDeletion(contact, nie::contentLastModified::iri());
    addressUpdate.addDeletion(imAddress, nco::presenceLastModified::iri());
    addressUpdate.addDeletion(imAddress, nco::imCapability::iri());

    addressUpdate.addInsertion(RDFStatementList() <<
                               RDFStatement(imAddress, rdf::type::iri(), nco::IMAddress::iri()) <<
                               RDFStatement(imAddress, nco::imNickname::iri(), LiteralValue(nick)) <<
                               RDFStatement(imAddress, nco::imStatusMessage::iri(), LiteralValue((msg))) <<
                               RDFStatement(imAddress, nco::imPresence::iri(), status) <<
                               RDFStatement(imAddress, nco::imID::iri(), LiteralValue(imId)) <<
                               RDFStatement(imAddress, nco::presenceLastModified::iri(), RDFVariable(datetime)));

    addressUpdate.addInsertion(RDFStatementList() <<
                               RDFStatement(contact, rdf::type::iri(), nco::PersonContact::iri()) <<
                               RDFStatement(contact, nco::hasIMAddress::iri(), imAddress) <<
                               RDFStatement(contact, nco::contactLocalUID::iri(), LiteralValue(id)) <<
                               RDFStatement(contact, nco::contactUID::iri(), LiteralValue(id)) );

    addressUpdate.addInsertion(RDFStatementList() <<
                               RDFStatement(imAccount, rdf::type::iri(), nco::IMAccount::iri()) <<
                               RDFStatement(imAccount, nco::hasIMContact::iri(), imAddress));

    addressUpdate.addInsertion(contact, nie::contentLastModified::iri(), RDFVariable(datetime));

    if (tpContact->supportsMediaCalls() || tpContact->supportsAudioCalls())  {
        addressUpdate.addInsertion( RDFStatementList() <<
                                    RDFStatement(imAddress, nco::imCapability::iri(),
                                                 nco::im_capability_audio_calls::iri()));

    }

    if (tpContact->supportsTextChats()) {
        addressUpdate.addInsertion( RDFStatementList() <<
                                    RDFStatement(imAddress, nco::imCapability::iri(),
                                                 nco::im_capability_text_chat::iri()));
    }

    service()->executeQuery(addressUpdate);
}

void CDTpTrackerSink::sinkToStorage(const QSharedPointer<CDTpContact>& obj)
{
    if (obj->contact() && obj->contact()->isBlocked()) {
        return;
    }

    const unsigned int uniqueId = obj->uniqueId();
    if (!find(uniqueId) ) {
        connectOnSignals(obj);
        d->contactMap[uniqueId] = obj;
    }


    if (!obj->isReady()) {
        connect(obj.data(), SIGNAL(ready(CDTpContact*)),
                this, SLOT(onFeaturesReady(CDTpContact*)));
        return;
    }


    qDebug() << Q_FUNC_INFO <<
        " \n{Contact Id :" << obj->id() <<
        " }\n{Alias : " <<  obj->alias() <<
        " }\n{Status : " << obj->presenceType() <<
        " }\n{Message : " << obj->presenceMessage() <<
        " }\n{AccountPath : " << obj->accountPath();

    const QString id = QString::number(contactLocalUID(obj.data()));

    saveToTracker(id, obj.data());
}

void CDTpTrackerSink::onCapabilities(CDTpContact* obj)
{
    if (!isValidCDTpContact(obj)) {
        qDebug() << Q_FUNC_INFO << "Invalid Telepathy Contact";
        return;
    }

    const RDFVariable imAddress(obj->imAddress());

    RDFUpdate addressUpdate;

    addressUpdate.addDeletion(imAddress, nco::imCapability::iri());

    if (obj->supportsAudioCalls() || obj->supportsMediaCalls()) {

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
}

// uniqueId - QContactLocalId calculated as CDTpContact::uniqueId() in CDTpContact::onSimplePresenceChanged
void CDTpTrackerSink::onSimplePresenceChanged(CDTpContact* obj)
{
    qDebug() << Q_FUNC_INFO;
    if (!isValidCDTpContact(obj)) {
        qDebug() << Q_FUNC_INFO << "Invalid Telepathy Contact";
        return;
    }

    const RDFVariable imAddress(obj->imAddress());
    const QDateTime datetime = QDateTime::currentDateTime();

    RDFUpdate addressUpdate;

    addressUpdate.addDeletion(imAddress, nco::imPresence::iri());
    addressUpdate.addDeletion(imAddress, nco::imStatusMessage::iri());
    addressUpdate.addDeletion(imAddress, nco::presenceLastModified::iri());

    RDFStatementList insertions;
    insertions << RDFStatement(imAddress, nco::imStatusMessage::iri(),
                               LiteralValue(obj->presenceMessage()));
    insertions << RDFStatement(imAddress, nco::imPresence::iri(),
                               toTrackerStatus(obj->presenceType()));
    insertions << RDFStatement(imAddress, nco::presenceLastModified::iri(),
                               RDFVariable(datetime));
    addressUpdate.addInsertion(insertions);

    service()->executeQuery(addressUpdate);
}

QList<QSharedPointer<CDTpContact> > CDTpTrackerSink::getFromStorage()
{
    return  d->contactMap.values();
}

bool CDTpTrackerSink::contains(const QString& aId)
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

bool CDTpTrackerSink::compareAvatar(const QString& token)
{
    Q_UNUSED(token)
    return false;
}


void CDTpTrackerSink::saveAvatarToken(const QString& id, const QString& token, const QString& mime)
{
    const QString avatarPath = CDTpContactPhotoCopy::avatarDir()+"/telepathy_cache"+token+'.'+ mime;
    qDebug() << Q_FUNC_INFO << token;

    foreach (const QSharedPointer<const CDTpContact>& c, d->contactMap) {
        const QSharedPointer<const Tp::Contact> tcontact = c->contact();
        if (tcontact && id == tcontact->id() ) {
            using namespace SopranoLive;
            if (!isValidCDTpContact(c.data())) {
                continue;
            }

            const QUrl tpUrl = c->imAddress();
            Live<nco::IMAddress> address = service()->liveNode(tpUrl);

            const unsigned int contactLocalId = contactLocalUID(c.data());
            const QString contactLocalIdString = QString::number(contactLocalId);
            Live<nco::PersonContact> photoAccount = service()->liveNode(buildContactIri(contactLocalId));

            // set both properties for a transition period
            // TODO: Set just one when it has been decided:
            photoAccount->setContactLocalUID(contactLocalIdString);
            photoAccount->setContactUID(contactLocalIdString);

            const QDateTime datetime = QDateTime::currentDateTime();
            photoAccount->setContentCreated(datetime);
            address->setPresenceLastModified(datetime);

            //FIXME:
            //To be removed once tracker plugin reads imaddress photo url's
            Live<nie::DataObject> fileurl = ::tracker()->liveNode(QUrl(avatarPath));
            photoAccount->setPhoto(fileurl);
            address->addImAvatar(fileurl);

            break;
        }
    }
}

void CDTpTrackerSink::onAvatarUpdated(const QString& id, const QString& token, const QString& mime)
{
    if (!compareAvatar(token))
        saveAvatarToken(id, token, mime);
}

//TODO: This seems to be useless:
bool CDTpTrackerSink::isValidCDTpContact(const CDTpContact *tpContact) {
    if (tpContact != 0) {
        return true;
    }

    return false;
}

RDFServicePtr CDTpTrackerSink::service()
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

void CDTpTrackerSink::commitTrackerTransaction() {
    if ( d->transaction_)
    {
        d->transaction_->commit();
        d->transaction_ = RDFTransactionPtr(0); // that's it, transaction lives until commit
    }
}

void CDTpTrackerSink::initiateTrackerTransaction()
{
    if ( !d->transaction_ )
        d->transaction_ = ::tracker()->createTransaction();
}


void CDTpTrackerSink::deleteContacts(const QString& path)
{
    //TODO: 
    //Fix the query when doing contact merging

    d->accountToDelete = path;
    RDFSelect select;

    RDFVariable contact = RDFVariable::fromType<nco::PersonContact>();
    RDFVariable imaddress = contact.optional().property<nco::hasIMAddress>();
    RDFVariable imaccount = RDFVariable::fromType<nco::IMAccount>();

    imaccount.property<nco::hasIMContact>() = imaddress;

    select.addColumn("contact", contact);
    select.addColumn("distinct", imaddress.property<nco::imID>());
    select.addColumn("contactId", contact.property<nco::contactLocalUID> ());
    select.addColumn("accountPath", imaccount);
    select.addColumn("address", imaddress);

    d->livenode = ::tracker()->modelQuery(select);

    connect(d->livenode.model(), SIGNAL(modelUpdated()), this, SLOT(onDeleteModelReady()));

}

void CDTpTrackerSink::deleteContact(const QSharedPointer<CDTpContact>& c)
{
    const RDFVariable imAddress(CDTpContact::buildImAddress(c->accountPath(), c->id()));

    RDFUpdate deleteContact;
    deleteContact.addDeletion(imAddress, rdf::type::iri());

    service()->executeQuery(deleteContact);
}

void CDTpTrackerSink::onDeleteModelReady()
{
     for (int i = 0 ; i < d->livenode->rowCount() ; i ++) {
        const QString imAddress = d->livenode->index(i, 1).data().toString();
        const QString imLocalId = d->livenode->index(i, 2).data().toString();
        const QString imAccountPath = d->livenode->index(i, 3).data().toString();
        const QString path(imAccountPath.split(":").value(1));

        if ( path == d->accountToDelete) {
            Live<nco::PersonContact> contact = d->livenode->liveResource<nco::PersonContact>(i, 0);
            contact->remove();
            Live<nco::IMAddress> address = d->livenode->liveResource<nco::PersonContact>(i,4);
            address->remove();
        }
     }

}

void CDTpTrackerSink::clearContacts(const QString& path)
{
    const QList<uint> list (d->contactMap.keys());
    
    foreach (uint index, list) {
        QSharedPointer<CDTpContact> contact = d->contactMap[index];
        
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

void CDTpTrackerSink::takeAllOffline(const QString& path)
{
    RDFUpdate addressUpdate;
    foreach (CDTpContactPtr obj, getFromStorage()) {
        if (obj->accountPath() != path) {
            continue;
        }

        const RDFVariable imAddress(obj->imAddress());

        addressUpdate.addDeletion(imAddress, nco::imPresence::iri());
        addressUpdate.addDeletion(imAddress, nco::imStatusMessage::iri());
        addressUpdate.addDeletion(imAddress, nco::presenceLastModified::iri());

        const QLatin1String status("unknown");
        addressUpdate.addInsertion(RDFStatementList() <<
                                   RDFStatement(imAddress, nco::imStatusMessage::iri(),
                                                LiteralValue("")) <<
                                   RDFStatement(imAddress, nco::imPresence::iri(),
                                                toTrackerStatus(status)));

        addressUpdate.addInsertion(imAddress, nco::presenceLastModified::iri(),
                                   RDFVariable(QDateTime::currentDateTime()));
    }

    service()->executeQuery(addressUpdate);
}

const QUrl & CDTpTrackerSink::toTrackerStatus(const uint stat)
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

const QUrl & CDTpTrackerSink::toTrackerStatus(const QString& status)
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

unsigned int CDTpTrackerSink::contactLocalUID(const CDTpContact* const tpContact, bool *existing) const
{
    Q_UNUSED(existing)
    return tpContact->uniqueId();
}
