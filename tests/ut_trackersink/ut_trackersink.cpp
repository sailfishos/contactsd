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
    manager(0)
{
}

void ut_trackersink::initTestCase()
{
    manager = new QContactManager(QLatin1String("tracker"));
    connect(manager, SIGNAL(contactsAdded(const QList<QContactLocalId>&)), this, SLOT(contactsAdded(const QList<QContactLocalId>&)));
    connect(manager, SIGNAL(contactsChanged(const QList<QContactLocalId>&)), this, SLOT(contactsChanged(const QList<QContactLocalId>&)));

    telepathier = qobject_cast<TpContactStub*>(TpContactStub::generateRandomContacts(1)[0]);

    QVERIFY(telepathier);
    // display name is mandatory detail when qtcontacts-tracker is reading contacts
    Live<nco::IMAccount> liveAccount = ::tracker()->liveNode(QUrl("telepathy:"+telepathier->accountPath()));
    liveAccount->setImDisplayName(QLatin1String("testService"));
}

void ut_trackersink::testSinkToStorage()
{
    QDateTime datetime = QDateTime::currentDateTime();
    // continue with telepathier sink
    sink->saveToTracker(telepathier->id(), telepathier);
    // check what was written

    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName, QContactOnlineAccount::FieldAccountUri);
    filter.setValue(telepathier->id());
    filter.setMatchFlags(QContactFilter::MatchExactly);

    QStringList details;
    details << QContactName::DefinitionName << QContactAvatar::DefinitionName
            << QContactOnlineAccount::DefinitionName << QContactPresence::DefinitionName;
    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(details);
    QList<QContact> contacts = manager->contacts(filter, QList<QContactSortOrder>(), fetchHint);
    QVERIFY(contacts.count() == 1);
    QContact contact(contacts[0]);

    contactInTrackerUID = contacts[0].localId();

    QStringList caps = contact.detail<QContactOnlineAccount>().capabilities();

    foreach(const QString &cap, telepathier->mCapabilities)
    {
        QVERIFY(caps.contains(cap));
    }

    QVERIFY(contact.detail<QContactPresence>().customMessage() == telepathier->presenceMessage());
    QVERIFY(contact.detail<QContactPresence>().nickname() == telepathier->alias());
    QVERIFY((unsigned int)contact.detail<QContactPresence>().presenceState() == telepathier->presenceType());
    QVERIFY(contact.detail(QContactOnlineAccount::DefinitionName).value("AccountPath") == telepathier->accountPath());
    QVERIFY(contact.detail<QContactPresence>().timestamp() > datetime);
}


void ut_trackersink::testOnSimplePresenceChanged()
{
    // check storing and reading of all presences
    QDateTime datetime = QDateTime::currentDateTime();
    for (int i = 0; i < 8; i++) {
        QString presenceMessage = telepathier->presenceMessage() + QLatin1String("change");
        telepathier->setPresenceMessage(presenceMessage);
        unsigned int presenceType = (telepathier->presenceType() + 1) % 7;
        telepathier->setPresenceType(presenceType);
        sink->onSimplePresenceChanged(telepathier);
        QContact contact = manager->contact(contactInTrackerUID);
        QVERIFY(telepathier->presenceMessage() == presenceMessage);
        QVERIFY(telepathier->presenceType() == presenceType);

        QVERIFY(contact.detail<QContactPresence>().customMessage() == telepathier->presenceMessage());
        QVERIFY(contact.detail<QContactPresence>().presenceState() == telepathier->presenceType());
        QVERIFY(contact.detail<QContactPresence>().timestamp() > datetime);
        datetime = QDateTime::currentDateTime();
    }
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

void ut_trackersink::cleanupTestCase()
{
    delete telepathier;
    delete manager;
}

QTEST_MAIN(ut_trackersink)
