/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2013-2019 Jolla Ltd.
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

#ifndef CDSIMCONTROLLER_H
#define CDSIMCONTROLLER_H

#include <QtCore>

#include <QContactManager>
#include <QContact>
#include <QContactCollection>
#include <QContactFilter>

#include <QVersitReader>

#include <qofonomodem.h>
#include <qofonophonebook.h>
#include <qofonosimmanager.h>
#include <qofonomessagewaiting.h>
#include <qofonoextsiminfo.h>

#include <MGConfItem>

QTCONTACTS_USE_NAMESPACE
QTVERSIT_USE_NAMESPACE

class CDSimModemData;
class CDSimController : public QObject
{
    Q_OBJECT

public:
    explicit CDSimController(QObject *parent = 0, bool active = true);
    ~CDSimController();

    QContactManager &contactManager();
    QContactCollection contactCollection(const QString &modemPath) const;
    int modemIndex(const QString &modemPath) const;

    bool busy() const;

Q_SIGNALS:
    void busyChanged(bool);

public Q_SLOTS:
    void setModemPaths(const QStringList &paths);
    void transientImportConfigurationChanged();
    void modemReadyChanged(bool ready);

private:
    void updateBusy();
    void timerEvent(QTimerEvent *event);
    void removeObsoleteSimCollections();

private:
    friend class CDSimModemData;
    friend class TestSimPlugin;

    QContactManager m_manager;
    bool m_transientImport;
    bool m_busy;
    bool m_active;
    MGConfItem m_transientImportConf;
    QBasicTimer m_readyTimer;

    QMap<QString, CDSimModemData *> m_modems;
    QSet<QString> m_absentModemPaths;
    QStringList m_availableModems;
};

class CDSimModemData : public QObject
{
    Q_OBJECT

public:
    CDSimModemData(CDSimController *controller, const QString &modemPath);
    ~CDSimModemData();

    CDSimController *controller() const;
    QContactManager &manager() const;

    QString modemIdentifier() const;
    QString modemPath() const;
    QContactCollection contactCollection() const;
    QList<QContact> fetchContacts() const;

    bool ready() const;
    void setReady(bool ready);

    void updateBusy();

    static bool removeCollections(QContactManager *manager, const QList<QContactCollectionId> &collectionIds);

Q_SIGNALS:
    void readyChanged(bool ready);

public Q_SLOTS:
    void simStateChanged();
    void vcardDataAvailable(const QString &vcardData);
    void vcardReadFailed();
    void readerStateChanged(QVersitReader::State state);
    void voicemailConfigurationChanged();
    void phonebookValidChanged(bool valid);

public:
    void deactivateAllSimContacts();
    void removeAllSimContacts();
    void ensureSimContactsPresent();
    void updateVoicemailConfiguration();
    void performTransientImport();
    void initCollection();

    void timerEvent(QTimerEvent *event);

    QString m_modemPath;
    QOfonoSimManager m_simManager;
    QOfonoPhonebook m_phonebook;
    QOfonoMessageWaiting m_messageWaiting;
    QOfonoExtSimInfo m_simInfo;
    MGConfItem *m_voicemailConf;
    QVersitReader m_contactReader;
    QList<QContact> m_simContacts;
    QContactCollection m_collection;
    QBasicTimer m_retryTimer;
    bool m_ready;
    int m_retries;
};

#endif // CDSIMCONTROLLER_H
