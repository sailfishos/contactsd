/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2014 - 2019 Jolla Ltd
 ** Copyright (c) 2020 Open Mobile Platform LLC.
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **/

#ifndef CDEXPORTERCONTROLLER_H
#define CDEXPORTERCONTROLLER_H

#include <QObject>
#include <QMultiHash>
#include <QHash>

#include <QContactManager>

#include <Accounts/Account>
#include <Accounts/Manager>

QTCONTACTS_USE_NAMESPACE

class CDExporterController : public QObject
{
    Q_OBJECT

public:
    explicit CDExporterController(QObject *parent = nullptr);
    ~CDExporterController();

private Q_SLOTS:
    void accountUpdated(const Accounts::AccountId &accountId);
    void collectionsAdded(const QList<QContactCollectionId> &collectionIds);
    void collectionContactsChanged(const QList<QContactCollectionId> &collectionIds);

private:
    QContactManager m_privilegedManager;
    Accounts::Manager *m_manager = nullptr;
    QMultiHash<Accounts::AccountId, QContactCollectionId> m_accountCollections;
    QHash<QContactCollectionId, Accounts::AccountId> m_collectionAccount;
};

#endif // CDEXPORTERCONTROLLER_H
