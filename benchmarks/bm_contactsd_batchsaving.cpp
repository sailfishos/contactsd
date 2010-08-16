/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QCoreApplication>
#include <QDebug>

#include <tpcontactstub.h>
#include <trackersink.h>

static const unsigned int MAX_CONTACTS = 5000;
static const unsigned int BATCH_SIZE = 1;

class Eng : public QObject
{
    Q_OBJECT;

public:
    Eng(unsigned int contactsCount, unsigned int bsizeCount) :
            count(contactsCount),
            batchSize(bsizeCount)
    {
        contacts = generateContacts(count);
    }

    virtual ~Eng()
    {
        qDeleteAll(contacts.begin(), contacts.end());
    }

    void performBatch()
    {
        unsigned int todo = count;
        unsigned int bsize;
        batchSize = 1; //TODO no batch saving yet
        do {
            bsize = (todo > batchSize) ? batchSize : todo;
            // use TpContact::id() as ContactLocalUID in this benchmark
            TrackerSink::instance()->saveToTracker(contacts[todo-1]->id(), contacts[todo-1]);
            todo -= bsize;
        } while (0 < todo);
        emit done();
    }

signals:
    void done();

private slots:

private:
    QList<TpContact*> generateContacts(unsigned int contactsCount) const
    {
        QList<TpContact*> result;

        qDebug() << Q_FUNC_INFO << "Generating contacts";
        const QStringList capabilitiesConsts (QStringList()
                << TpContactStub::CAPABILITIES_TEXT
                << TpContactStub::CAPABILITIES_MEDIA);

        const unsigned int onlineStatusesMax = 7;

        for (unsigned i = 0; i < contactsCount; ++i)
        {
            TpContactStub *telepathier = new TpContactStub();

            // unique 32
            QUuid guid = QUuid::createUuid();
            QString str = guid.toString().replace(QRegExp("[-}{]"), "");
            str = str.right(8);
            quint32 id = str.toUInt(0, 16);

            telepathier->setId(QString::number(id, 10));

            const int capsCount = capabilitiesConsts.size();
            QStringList list;
            for (int cap = 0; cap < capsCount; cap++ ) {
                if (i%(cap+1)) {
                    list << capabilitiesConsts[cap];
                }
            }
            telepathier->setCapabilitiesStrings(list);

            telepathier->setPresenceMessage(QString::fromLatin1("bm_contactsd_batchsaving message for ")+telepathier->id());

            telepathier->setPresenceType(i%onlineStatusesMax);

            telepathier->setAccountPath(QString("gabble/jabber/bm_contactsd_batchsaving"));

            result << telepathier;
        }

        return result;
    }

private:
    unsigned count;
    unsigned batchSize;
    QList<TpContact*> contacts;
};

#include "bm_contactsd_batchsaving.moc"

int main(int argc, char* argv[])
{
    qsrand(time(0));

    QCoreApplication app(argc, argv);
    unsigned count = MAX_CONTACTS;
    unsigned bsize = BATCH_SIZE;

    if (QCoreApplication::arguments().count() >= 2) {
        bool success = false;
        count = QCoreApplication::arguments()[1].toUInt(&success);
        if (!success) count = MAX_CONTACTS;
    }

    if (QCoreApplication::arguments().count() >= 3) {
        bool success = false;
        bsize = QCoreApplication::arguments()[2].toUInt(&success);
        if (!success) bsize = BATCH_SIZE;
    }


    qDebug()
            << "Going to add" << count << "contacts in batches of (batch is not yet used in code)" << bsize;

    Eng eng(count, bsize);

    QObject::connect(&eng, SIGNAL(done()),
                     &app, SLOT(quit()), Qt::QueuedConnection);

    QTime t;
    t.start();
    eng.performBatch();
    qDebug("Time elapsed: %.3f", static_cast<double>(t.elapsed()) / 1000);

    return app.exec();
}
