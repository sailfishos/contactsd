/* * This file is part of contacts *
 * Copyright © 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
 * Contact: Aleksandar Stojiljkovic <aleksandar.stojiljkovic@nokia.com>
 * This software, including documentation, is protected by copyright controlled by
 * Nokia Corporation. All rights are reserved. Copying, including reproducing, storing,
 * adapting or translating, any or all of this material requires the prior written consent
 * of Nokia Corporation. This material also contains confidential information which may
 * not be disclosed to others without the prior written consent of Nokia. */

#include "ut_trackersink.h"

#include "trackersink.h"


#include <QMap>
#include <QPair>

#include <contactmanager.h>

#include <QtTracker/Tracker>
#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nie.h>
#include <qcontactfilters.h>
#include <qtcontacts.h>


ut_trackersink::ut_trackersink()
  : sink(TrackerSink::instance())
{
    connect(ContactManager::instance(), SIGNAL(contactsAdded(const QList<QContactLocalId>&)), this, SLOT(contactsAdded(const QList<QContactLocalId>&)));
    connect(ContactManager::instance(), SIGNAL(contactsChanged(const QList<QContactLocalId>&)), this, SLOT(contactsChanged(const QList<QContactLocalId>&)));
}

void ut_trackersink::initTestCase()
{
    telepathier = QSharedPointer<TelepathyContact>(new TpContact(),
                                         &QObject::deleteLater);
    // unique 32
    QUuid guid = QUuid::createUuid();
    QString str = guid.toString().replace(QRegExp("[-}{]"), "");
    str = str.right(8);
    quint32 id = str.toUInt(0, 16);
    telepathier->setId(QString::number(id, 10));
    Tp::ContactCapability cap;
    cap.channelType = "org.freedesktop.Telepathy.Channel.Type.StreamedMedia";
    Tp::ContactCapabilityList list(Tp::ContactCapabilityList()<<cap);
    Tp::ContactCapability cap1;
    cap1.channelType = "TestCap";
    list<<cap1;
    telepathier->mCaps = list;
    QCOMPARE(telepathier->id(),QString::number(id, 10) );
    telepathier->setMessage("message1"+telepathier->id());
    telepathier->setPresenceStatus("offline");
    telepathier->setAccountPath(QString("gabble/jabber/collabora_2etest_40gmail_2ecom1"));
    telepathier->_uniqueId = id;
    // TODO continuing with sink
}

void ut_trackersink::testSinkToStorage()
{
    // continue with telepathier sink
    sink->sinkToStorage(telepathier);
    // check what was written
    QContactFetchRequest request;
    request.setManager(ContactManager::instance());
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName, "Account");
    filter.setValue(telepathier->id());
    filter.setMatchFlags(QContactFilter::MatchExactly);

    QStringList details;
    details << QContactName::DefinitionName << QContactAvatar::DefinitionName
            << QContactOnlineAccount::DefinitionName;
    request.setDefinitionRestrictions(details);
    request.setFilter(filter);

    // start both at once
    request.start();
    QVERIFY(request.waitForFinished(2000));
    // if it takes more, then something is wrong
    QVERIFY(request.isFinished());

    QVERIFY(request.contacts().count() == 1);
    contactInTrackerUID = request.contacts()[0].localId();

    QString caps = request.contacts()[0].detail(QContactOnlineAccount::DefinitionName).value("Capabilities");
    foreach(Tp::ContactCapability cap, telepathier->mCaps)
    {
        QVERIFY(caps.contains(cap.channelType));
    }

    QVERIFY(request.contacts()[0].detail(QContactOnlineAccount::DefinitionName).value(QContactOnlineAccount::FieldStatusMessage) == telepathier->message());
    QVERIFY(request.contacts()[0].detail(QContactOnlineAccount::DefinitionName).value(QContactOnlineAccount::FieldPresence).contains(telepathier->presenceStatus(), Qt::CaseInsensitive));
    QVERIFY(request.contacts()[0].detail(QContactOnlineAccount::DefinitionName).value("AccountPath") == telepathier->accountPath());
}

void ut_trackersink::testOnSimplePresenceChanged()
{
    sink->onSimplePresenceChanged(telepathier.data(), "offline", 0, "new status"+telepathier->id());
    QContact contact = ContactManager::instance()->contact(contactInTrackerUID);
    QVERIFY(telepathier->message() == QString("new status"+telepathier->id()) );
    QVERIFY(telepathier->presenceStatus() == QString("offline"));

    QVERIFY(contact.detail(QContactOnlineAccount::DefinitionName).value(QContactOnlineAccount::FieldStatusMessage) == telepathier->message());
    QVERIFY(contact.detail(QContactOnlineAccount::DefinitionName).value(QContactOnlineAccount::FieldPresence).contains(telepathier->presenceStatus(), Qt::CaseInsensitive));
}

// Fetch the im account based on telepathy contact and personcontact
// and compare the result to make sure thay are the same.
void ut_trackersink::testImContactForTpContactId()
{
    //Find the imAccount that corresponds to the telepathy contact id.
    Live<nco::IMAccount> imAccount = sink->imAccountForTpContactId("0");

    //INSERT <test:0>
    Live<nco::PersonContact> ncoContact;
    ncoContact =
        ::tracker()->liveNode(QUrl("contact:0"));

    //DELETE contactLocalUID and INSERT contactLocalUID
    ncoContact->setContactLocalUID("0");

    Live<nco::IMAccount> imAccount1 = sink->imAccountForPersonContact( ncoContact );

    QCOMPARE( imAccount.iri(), imAccount1.iri() );
}

// In different order compared to the test above.
// Fetch the im account based on telepathy contact and personcontact
// and compare the result to make sure thay are the same.
void ut_trackersink::testImContactForPeople()
{
    //INSERT <test:0>
    Live<nco::PersonContact> ncoContact;
    ncoContact =
        ::tracker()->liveNode(QUrl("contact:0"));

    //DELETE contactLocalUID and INSERT contactLocalUID
    ncoContact->setContactLocalUID("0");

    //Find existing IMAccount or INSERT IMAccount.
    // With empty tracker this results in subject changed signal for
    // "<test:0>" "http://www.semanticdesktop.org/ontologies/2007/03/22/nco#hasIMAccount"
    Live<nco::IMAccount> imAccount1 = sink->imAccountForPersonContact( ncoContact );

    //Compare the ImAccount to the one returned for telepathycontact
    Live<nco::IMAccount> imAccount2 = sink->imAccountForTpContactId("0");
    QCOMPARE( imAccount1.iri(), imAccount2.iri() );
}

// Create a "new" Live node contact and check that the new contact is returned.
void ut_trackersink::testPersonContactForTpContactId()
{
    Live<nco::PersonContact> ncoContact;
    ncoContact =
        ::tracker()->liveNode(QUrl("contact:0"));

    Live<nco::PersonContact> foundNcoContact = sink->imContactForTpContactId("0");

    QCOMPARE( ncoContact.iri() , foundNcoContact.iri() );
}

void ut_trackersink::contactsAdded(const QList<QContactLocalId>& contactIds)
{
    added.append(contactIds);
    qDebug()<<Q_FUNC_INFO<<added;
}

void ut_trackersink::contactsChanged(const QList<QContactLocalId>& contactIds)
{
    qDebug()<<Q_FUNC_INFO;
    changed.append(contactIds);
}

void ut_trackersink::sinkContainsImAccount() {
    QString accountName = "nobody@foobar.baz";
    QVERIFY(sink->contains(accountName) == false);

    // As the TrackerSink uses Qt Tracker Transactions, we need
    // to flush the Transactions cache now in order to get this unit test
    // working.
    RDFTransactionPtr tx = ::tracker()->pendingTransaction();
    if(tx) {
        tx->commit();
    }

    Live<nco::PersonContact> ncoContact = ::tracker()->createLiveNode();
    Live<nco::IMAccount> im = ncoContact->addHasIMAccount();
    im->setImID(accountName);
    QVERIFY(sink->contains(accountName) == true);

    im->remove();
    ncoContact->remove();
    QVERIFY(sink->contains(accountName) == false);

    // Restore Transactions.
    QVERIFY(!tx || tx->reinitiate());
}

void Slots::progress(QContactFetchRequest* self, bool appendOnly)
{
    Q_UNUSED(appendOnly)
    contacts = self->contacts();
    QList<QContactLocalId> idsFromAllContactReq;
}

void ut_trackersink::cleanupTestCase()
{
    telepathier.clear();
    QVERIFY(telepathier.isNull());
    // see contacts::main() - the same bug here - application not closing
}

QTEST_MAIN(ut_trackersink)
