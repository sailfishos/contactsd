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

#ifndef TEST_EXPECTATION_H
#define TEST_EXPECTATION_H

#include <QObject>
#include <QString>
#include <QContactManager>
#include <QContact>

#include <telepathy-glib/telepathy-glib.h>
#include "libtelepathy/contacts-conn.h"


#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "fakecm/fakeproto/fakeaccount"

QTM_USE_NAMESPACE

typedef enum {
    EventAdded,
    EventChanged,
    EventRemoved
} Event;

// --- TestExpectation ---

class TestExpectation : public QObject
{
    Q_OBJECT

public:
    void verify(Event event, const QList<QContactLocalId> &contactIds);

    void setContactManager(QContactManager *contactManager) { mContactManager = contactManager; };
    QContactManager *contactManager() { return mContactManager; };

Q_SIGNALS:
    void finished();

protected:
    virtual void verify(Event event, const QList<QContact> &contacts);
    virtual void verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error);
    void emitFinished();

private:
    friend class TestFetchContacts;

    QContactManager *mContactManager;
};

// --- TestFetchContacts ---

class TestFetchContacts : public QObject
{
    Q_OBJECT

public:
    TestFetchContacts(const QList<QContactLocalId> &contactIds, Event event, TestExpectation *exp);

private Q_SLOTS:
    void onContactsFetched();

private:
    QList<QContactLocalId> mContactIds;
    Event mEvent;
    TestExpectation *mExp;
};

// --- TestExpectationInit ---

class TestExpectationInit : public TestExpectation
{
    Q_OBJECT

protected:
    void verify(Event event, const QList<QContact> &contacts);
};

// --- TestExpectationCleanup ---

class TestExpectationCleanup : public TestExpectation
{
    Q_OBJECT

public:
    TestExpectationCleanup(int nContacts);

protected:
    void verify(Event event, const QList<QContact> &contacts);
    void verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error);

private:
    void maybeEmitFinished();

private:
    int mNContacts;
    bool mSelfChanged;
};

// --- TestExpectationContact ---

class TestExpectationContact : public TestExpectation
{
    Q_OBJECT

public:
    TestExpectationContact(Event event, QString accountUri = QString());

    QContact contact() { return mContact; };
    void setEvent(Event event) { mEvent = event; };
    void resetVerifyFlags() { mFlags = 0; };

    void verifyAlias(QString alias) { mAlias = alias; mFlags |= VerifyAlias; };
    void verifyPresence(TpTestsContactsConnectionPresenceStatusIndex presence) { mPresence = presence; mFlags |= VerifyPresence; };
    void verifyAvatar(QByteArray avatarData) { mAvatarData = avatarData; mFlags |= VerifyAvatar; };
    void verifyAuthorization(QString subscriptionState, QString publishState) { mSubscriptionState = subscriptionState; mPublishState = publishState; mFlags |= VerifyAuthorization; };
    void verifyInfo(GPtrArray *contactInfo) { mContactInfo = contactInfo; mFlags |= VerifyInfo; };
    void verifyLocalId(QContactLocalId localId) { mLocalId = localId; mFlags |= VerifyLocalId; };

    void verify(QContact contact);

protected:
    void verify(Event event, const QList<QContact> &contacts);
    void verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error);

private:
    enum VerifyFlags {
        VerifyNone           = 0,
        VerifyAlias          = (1 << 0),
        VerifyPresence       = (1 << 1),
        VerifyAvatar         = (1 << 2),
        VerifyAuthorization  = (1 << 3),
        VerifyInfo           = (1 << 4),
        VerifyLocalId        = (1 << 5),
        VerifyAll            = (1 << 6) - 1
    };

    void verifyContactInfo(const QString name, const QStringList values, const QStringList params) const;

    QString mAccountUri;
    Event mEvent;
    int mFlags;

    QString mAlias;
    TpTestsContactsConnectionPresenceStatusIndex mPresence;
    QByteArray mAvatarData;
    QString mSubscriptionState;
    QString mPublishState;
    GPtrArray *mContactInfo;
    QContactLocalId mLocalId;

    QContact mContact;
};

// --- TestExpectationDisconnect ---

class TestExpectationDisconnect : public TestExpectationContact
{
    Q_OBJECT

public:
    TestExpectationDisconnect(int nContacts);

protected:
    void verify(Event event, const QList<QContact> &contacts);

private:
    int mNContacts;
    bool mSelfChanged;
};

// --- TestExpectationMerge ---

class TestExpectationMerge : public TestExpectation
{
    Q_OBJECT

public:
    TestExpectationMerge(const QContactLocalId masterId, const QList<QContactLocalId> mergeIds,
            const QList<TestExpectationContact *> expectations = QList<TestExpectationContact *>());

protected:
    void verify(Event event, const QList<QContact> &contacts);
    void verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error);

private:
    void maybeEmitFinished();

    QContactLocalId mMasterId;
    QList<QContactLocalId> mMergeIds;
    bool mGotMergedContact;
    QList<TestExpectationContact *> mContactExpectations;
};

// --- TestExpectationMass ---

class TestExpectationMass : public TestExpectation
{
    Q_OBJECT

public:
    TestExpectationMass(int nAdded, int nChanged, int nRemoved);

protected:
    void verify(Event event, const QList<QContact> &contacts);
    void verify(Event event, const QList<QContactLocalId> &contactIds, QContactManager::Error error);

private:
    void maybeEmitFinished();

    int mAdded;
    int mChanged;
    int mRemoved;
};

#endif
