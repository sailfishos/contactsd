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

#ifndef TPCONTACTSTUB_H
#define TPCONTACTSTUB_H

// Telepathy
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/Connection>
#include <TelepathyQt4/ContactCapabilities>
// Qt
#include <tpcontact.h>



/*!
 * \class
 */
class TpContactStub : public TpContact
{
    Q_OBJECT
public:
    /*! \brief Contstucts a Telepathy Contact which is
      * writable using an existing Tp::Contact
      * \param contact which is meant to be wrapped
      * \param id the group id which this contact should belong to
      */
    explicit TpContactStub(QObject * parent = 0);
    virtual ~TpContactStub();

    //!\ Used in tests to generate content
    static QList<TpContact*> generateRandomContacts(unsigned int contactsCount);

    //!\reimp
    bool supportsTextChats() const;
    bool supportsMediaCalls() const;
    bool supportsAudioCalls() const;
    QString id() const;
    QString alias() const;
    unsigned int presenceType() const;
    QString presenceMessage() const;
    //!\reimp_end

    void setId(const QString& Id);
    void setAlias(const QString& Alias);
    void setPresenceMessage(const QString&);
    void setPresenceType(const unsigned int);
    void setAvatarToken(const QString&, const QString&);
    /*!
     * This looks like avatar token and it is used to track for which contact
     * avatar is being set
     */
    QString avatar() const;
    /*!
     * avatar file path. If avatar image has not yet been loaded, returns empty string
     */
    QString avatarFilePath() const;

    void setAccountPath(const QString&);
    QString accountPath() const;

    Tp::ContactPtr contact() const;

    //!\ fill following values to \sa setCapabilitiesStrings to define capabilities
    static const QString CAPABILITIES_TEXT;
    static const QString CAPABILITIES_MEDIA;

    void setCapabilitiesStrings(const QStringList &);

private:
    Tp::ContactPtr mContact;
    QString mId, mPresenceMessage, mAlias;
    unsigned int mPresenceType;
    QString mAvatar;
    QString mAccountPath;
    QStringList mCapabilities;

    // This _uniqueId is used for tests. test set it so that it is not fetched fro
    // somewhere else
    unsigned int _uniqueId;

    friend class ut_trackersink;
};

typedef QList<QSharedPointer<TpContactStub> > ContactList;
typedef QSharedPointer<TpContactStub> TpContactStubPtr;

#endif // TPCONTACTSTUBH
