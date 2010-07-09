#ifndef PENDINGROSTERS_H
#define PENDINGROSTERS_H

#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/Types>
#include <TelepathyQt4/Account>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/PendingContacts>
#include <TelepathyQt4/PendingAccount>
#include <TelepathyQt4/ContactManager>
#include <TelepathyQt4/ConnectionManager>
#include <QList>
#include <tpcontact.h>

class TelepathyController;

class PendingRosters : public Tp::PendingOperation
{
    Q_OBJECT
public:
    explicit PendingRosters(QObject *parent = 0);
    Tp::Contacts rosterList();
    QList<QSharedPointer<TpContact> > telepathyRosterList();
Q_SIGNALS:
    void contact(QSharedPointer<TpContact>);
private Q_SLOTS:
    void onConnectionReady(Tp::PendingOperation * po);
    void onAccountReady(Tp::PendingOperation* op);
    void onHaveConnectionChanged(bool);
    void onConnectionStatusChanged(Tp::ConnectionStatus status, Tp::ConnectionStatusReason reason);
private:
    Q_DISABLE_COPY(PendingRosters)
    friend class TelepathyController;
    void addRequestForAccount(Tp::AccountPtr);

    Tp::Contacts mContacts;
    QList<QSharedPointer<TpContact> >  mTpContacts;
    QList<Tp::ConnectionPtr> mConnectionList;
    Tp::AccountPtr mAccount;
    int mCapCount;
    bool mWasOnline;

};

#endif // PENDINGROSTERS_H
