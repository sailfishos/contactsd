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

#include "ut_trackersink.h"

#include "trackersink.h"


#include <QMap>
#include <QPair>

#include <QtTracker/Tracker>
#include <QtTracker/ontologies/nco.h>
#include <QtTracker/ontologies/nie.h>
#include <qcontactfilters.h>
#include <qtcontacts.h>


ut_trackersink::ut_trackersink() :
    sink(TrackerSink::instance()),
    manager(new QContactManager(QLatin1String("tracker")))
{
    connect(manager, SIGNAL(contactsAdded(const QList<QContactLocalId>&)), this, SLOT(contactsAdded(const QList<QContactLocalId>&)));
    connect(manager, SIGNAL(contactsChanged(const QList<QContactLocalId>&)), this, SLOT(contactsChanged(const QList<QContactLocalId>&)));
}

void ut_trackersink::initTestCase()
{
    telepathier = qobject_cast<TpContactStub*>(TpContactStub::generateRandomContacts(1)[0]);
    QVERIFY(telepathier);
}

void ut_trackersink::testSinkToStorage()
{
    // continue with telepathier sink
    sink->saveToTracker(telepathier->id(), telepathier);
    // check what was written

    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName, QContactOnlineAccount::FieldAccountUri);
    filter.setValue(telepathier->id());
    filter.setMatchFlags(QContactFilter::MatchExactly);

    QStringList details;
    details << QContactName::DefinitionName << QContactAvatar::DefinitionName
            << QContactOnlineAccount::DefinitionName;
    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(details);
    QList<QContact> contacts = manager->contacts(filter, QList<QContactSortOrder>(), fetchHint);
    QVERIFY(contacts.count() == 1);
    QContact contact(contacts[0]);

    contactInTrackerUID = contacts[0].localId();

    QStringList caps = contact.detail<QContactOnlineAccount>().capabilities();
    qDebug() << Q_FUNC_INFO << caps;
    foreach(const QString &cap, telepathier->mCapabilities)
    {
        QVERIFY(caps.contains(cap));
    }

    QVERIFY(contact.detail<QContactPresence>().customMessage() == telepathier->presenceMessage());
    QVERIFY(contact.detail<QContactPresence>().nickname() == telepathier->alias());
    QVERIFY((unsigned int)contact.detail<QContactPresence>().presenceState() == telepathier->presenceType());
    QVERIFY(contact.detail(QContactOnlineAccount::DefinitionName).value("AccountPath") == telepathier->accountPath());
}
/*
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
*/

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

void ut_trackersink::cleanupTestCase()
{
    delete telepathier;
    delete manager;
}

QTEST_MAIN(ut_trackersink)
