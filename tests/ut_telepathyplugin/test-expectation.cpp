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

#include <QTest>
#include <QFile>

#include <QContactFetchByIdRequest>
#include <QContactAvatar>
#include <QContactOnlineAccount>
#include <QContactPhoneNumber>
#include <QContactAddress>
#include <QContactPresence>
#include <QContactEmailAddress>

#include "test-expectation.h"
#include "debug.h"

// --- TestExpectation ---

void TestExpectation::verify(Event event, const QList<QContactLocalId> &contactIds)
{
     new TestFetchContacts(contactIds, event, this);
}

void TestExpectation::verify(Event event, const QList<QContact> &contacts)
{
    Q_UNUSED(event);
    Q_UNUSED(contacts);

    emitFinished();
}

void TestExpectation::verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error)
{
    Q_UNUSED(event);
    Q_UNUSED(contactIds);
    Q_UNUSED(error);

    QVERIFY2(false, "Error fetching contacts");
    emitFinished();
}

void TestExpectation::emitFinished()
{
    Q_EMIT finished();
}

// --- TestFetchContacts ---

TestFetchContacts::TestFetchContacts(const QList<QContactLocalId> &contactIds,
        Event event, TestExpectation *exp) : QObject(exp),
        mContactIds(contactIds), mEvent(event), mExp(exp)
{
    QContactFetchByIdRequest *request = new QContactFetchByIdRequest();
    connect(request, SIGNAL(resultsAvailable()),
        SLOT(onContactsFetched()));
    request->setManager(mExp->contactManager());
    request->setLocalIds(contactIds);
    QVERIFY(request->start());
}

void TestFetchContacts::onContactsFetched()
{
    QContactFetchByIdRequest *req = qobject_cast<QContactFetchByIdRequest *>(sender());
    if (req == 0 || !req->isFinished()) {
        return;
    }

    if (req->error() == QContactManager::NoError) {
        mExp->verify(mEvent, req->contacts());
    } else {
        mExp->verify(mEvent, mContactIds, req->error());
    }

    deleteLater();
}

// --- TestExpectationInit ---

void TestExpectationInit::verify(Event event, const QList<QContact> &contacts)
{
    QCOMPARE(event, EventChanged);
    QCOMPARE(contacts.count(), 1);
    QCOMPARE(contacts[0].localId(), contactManager()->selfContactId());
    emitFinished();
}

// --- TestExpectationCleanup ---

TestExpectationCleanup::TestExpectationCleanup(int nContacts) :
        mNContacts(nContacts), mSelfChanged(false)
{
}

void TestExpectationCleanup::verify(Event event, const QList<QContact> &contacts)
{
    QCOMPARE(event, EventChanged);
    QCOMPARE(contacts.count(), 1);
    QCOMPARE(contacts[0].localId(), contactManager()->selfContactId());
    mNContacts--;
    mSelfChanged = true;

    maybeEmitFinished();
}

void TestExpectationCleanup::verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error)
{
    QCOMPARE(event, EventRemoved);
    QCOMPARE(error, QContactManager::DoesNotExistError);
    mNContacts -= contactIds.count();

    maybeEmitFinished();
}

void TestExpectationCleanup::maybeEmitFinished()
{
    QVERIFY(mNContacts >= 0);
    if (mNContacts == 0 && mSelfChanged) {
        emitFinished();
    }
}

// --- TestExpectationContact ---

TestExpectationContact::TestExpectationContact(Event event):
        mEvent(event), mFlags(0), mPresence(TP_TESTS_CONTACTS_CONNECTION_STATUS_UNSET),
        mContactInfo(0), mNOnlineAccounts(0)
{
}

void TestExpectationContact::verify(Event event, const QList<QContact> &contacts)
{
    QCOMPARE(event, mEvent);
    QCOMPARE(contacts.count(), 1);
    verify(contacts[0]);
    emitFinished();
}

void TestExpectationContact::verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error)
{
    QCOMPARE(event, EventRemoved);
    QCOMPARE(contactIds.count(), 1);
    QCOMPARE(error, QContactManager::DoesNotExistError);
    emitFinished();
}

void TestExpectationContact::verify(QContact contact)
{
    debug() << contact;

    if (mFlags & VerifyOnlineAccounts) {
        QCOMPARE(contact.details<QContactOnlineAccount>().count(), mNOnlineAccounts);
        QCOMPARE(contact.details<QContactPresence>().count(), mNOnlineAccounts);
        if (mNOnlineAccounts == 1) {
            QCOMPARE(contact.detail<QContactOnlineAccount>().accountUri(), mAccountUri);
            QCOMPARE(contact.detail<QContactOnlineAccount>().value("AccountPath"), QString(ACCOUNT_PATH));
        }
    }

    if (mFlags & VerifyAlias) {
        QString alias = contact.detail<QContactDisplayLabel>().label();
        QCOMPARE(alias, mAlias);
    }

    if (mFlags & VerifyPresence) {
        QContactPresence::PresenceState presence = contact.detail<QContactPresence>().presenceState();
        switch (mPresence) {
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE:
            QCOMPARE(presence, QContactPresence::PresenceAvailable);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY:
            QCOMPARE(presence, QContactPresence::PresenceBusy);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY:
            QCOMPARE(presence, QContactPresence::PresenceAway);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_OFFLINE:
            QCOMPARE(presence, QContactPresence::PresenceOffline);
            break;
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN:
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_ERROR:
        case TP_TESTS_CONTACTS_CONNECTION_STATUS_UNSET:
            QCOMPARE(presence, QContactPresence::PresenceUnknown);
            break;
        }
    }

    if (mFlags & VerifyAvatar) {
        QString avatarFileName = contact.detail<QContactAvatar>().imageUrl().path();
        if (mAvatarData.isEmpty()) {
            QVERIFY2(avatarFileName.isEmpty(), "Expected empty avatar filename");
        } else {
            QFile file(avatarFileName);
            file.open(QIODevice::ReadOnly);
            QCOMPARE(file.readAll(), mAvatarData);
            file.close();
        }
    }

    if (mFlags & VerifyAuthorization) {
        QContactPresence presence = contact.detail<QContactPresence>();
        QCOMPARE(presence.value("AuthStatusFrom"), mSubscriptionState);
        QCOMPARE(presence.value("AuthStatusTo"), mPublishState);
    }

    if (mFlags & VerifyInfo) {
        uint nMatchedField = 0;

        Q_FOREACH (const QContactDetail &detail, contact.details()) {
            if (detail.definitionName() == "PhoneNumber") {
                QContactPhoneNumber phoneNumber = static_cast<QContactPhoneNumber>(detail);
                verifyContactInfo("tel", QStringList() << phoneNumber.number());
                nMatchedField++;
            }
            else if (detail.definitionName() == "Address") {
                QContactAddress address = static_cast<QContactAddress>(detail);
                verifyContactInfo("adr", QStringList() << address.postOfficeBox()
                                                       << QString("unmapped") // extended address is not mapped
                                                       << address.street()
                                                       << address.locality()
                                                       << address.region()
                                                       << address.postcode()
                                                       << address.country());
                nMatchedField++;
            }
            else if (detail.definitionName() == "EmailAddress") {
                QContactEmailAddress emailAddress = static_cast<QContactEmailAddress >(detail);
                verifyContactInfo("email", QStringList() << emailAddress.emailAddress());
                nMatchedField++;
            }
        }

        if (mContactInfo != NULL) {
            QCOMPARE(nMatchedField, mContactInfo->len);
        }
    }
}

void TestExpectationContact::verifyContactInfo(QString name, const QStringList values) const
{
    QVERIFY2(mContactInfo != NULL, "Found ContactInfo field, was expecting none");

    bool found = false;
    for (uint i = 0; i < mContactInfo->len; i++) {
        gchar *c_name;
        gchar **c_parameters;
        gchar **c_values;

        tp_value_array_unpack((GValueArray*) g_ptr_array_index(mContactInfo, i),
            3, &c_name, &c_parameters, &c_values);

        /* if c_values_len < values.count() it could still be OK if all
         * additional values are empty. */
        gint c_values_len = g_strv_length(c_values);
        if (QString(c_name) != name || c_values_len > values.count()) {
            continue;
        }

        bool match = true;
        for (int j = 0; j < values.count(); j++) {
            if (values[j] == QString("unmapped")) {
                continue;
            }
            if ((j < c_values_len && values[j] != QString(c_values[j])) ||
                (j >= c_values_len && !values[j].isEmpty())) {
                match = false;
                break;
            }
        }

        if (match) {
            found = true;
            break;
        }
    }

    QVERIFY2(found, "Unexpected ContactInfo field");
}

// --- TestExpectationDisconnect ---

TestExpectationDisconnect::TestExpectationDisconnect(int nContacts) :
        TestExpectationContact(EventChanged), mNContacts(nContacts), mSelfChanged(false)
{
}

void TestExpectationDisconnect::verify(Event event, const QList<QContact> &contacts)
{
    QCOMPARE(event, EventChanged);

    Q_FOREACH (const QContact contact, contacts) {
        if (contact.localId() == contactManager()->selfContactId()) {
            verifyPresence(TP_TESTS_CONTACTS_CONNECTION_STATUS_OFFLINE);
            mSelfChanged = true;
        } else {
            verifyPresence(TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN);
        }
        TestExpectationContact::verify(contact);
    }

    mNContacts -= contacts.count();
    QVERIFY(mNContacts >= 0);
    if (mNContacts == 0 && mSelfChanged) {
        emitFinished();
    }
}
