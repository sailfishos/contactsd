#include "pendingrosters.h"

/*! \class PendingRosters
 * \brief Pending Operation for getting the roster contacts from a given account
 *
 * This class works like another PendingOperation from Telepathy Qt4, When finished
 * signal is emitted the class will contain the rosters for the account which the
 * pendingroster class was created for
 * */


/*! \fn PendingRosters::addRequestForAccount(Tp::AccountPtr account)

  \brief Adds a pending Roster request for a given account

  \param account Account to get the Rosters from
  \sa TelepathyController::requestRosters(Tp::AccountPtr account)
*/


PendingRosters::PendingRosters(QObject * parent):Tp::PendingOperation(parent)
{
    mCapCount = 0;
    mWasOnline = false;
}

void PendingRosters::addRequestForAccount(Tp::AccountPtr account)
{
    mAccount = account;
    qDebug() << Q_FUNC_INFO << ": Waiting for account to become ready: " << account->objectPath();
    connect(account->becomeReady(), SIGNAL(finished(Tp::PendingOperation *)),
            this, SLOT(onAccountReady(Tp::PendingOperation *)));
}

void PendingRosters::onAccountReady(Tp::PendingOperation* op)
{
    if (op->isError()) {
        setFinishedWithError(op->errorName(), op->errorMessage());
        return;
    }

    Tp::PendingReady * pa = qobject_cast<Tp::PendingReady *>(op);
    Tp::AccountPtr account;
    if (pa) {
        account = Tp::AccountPtr(qobject_cast<Tp::Account *>(pa->object()));
    } else {
        account = mAccount;
    }

    qDebug() << Q_FUNC_INFO << ": Account is ready: " << account->objectPath();

    if (account) {
        if (account->haveConnection()) {
            qDebug() << Q_FUNC_INFO << ": Requesting connection for account: " << account->objectPath();

            mWasOnline = true;
            Tp::ConnectionPtr connection = account->connection();
            connect(connection->becomeReady(Tp::Connection::FeatureRoster),
                    SIGNAL(finished(Tp::PendingOperation *)),
                    SLOT(onConnectionReady(Tp::PendingOperation *)));
        } else {
            connect(account.data(), SIGNAL(connectionStatusChanged(Tp::ConnectionStatus,
                                           Tp::ConnectionStatusReason)), this
                    , SLOT(onConnectionStatusChanged(Tp::ConnectionStatus,
                                                     Tp::ConnectionStatusReason)));
            qDebug() << Q_FUNC_INFO << ": Account :" << account->objectPath() << " is not online.";
        }
    }

}

void PendingRosters::onConnectionStatusChanged(Tp::ConnectionStatus status, Tp::ConnectionStatusReason reason)
{
    Q_UNUSED(reason)

    if (status == Tp::ConnectionStatusConnected) {
        onHaveConnectionChanged(true);
    }

    if (status == Tp::ConnectionStatusDisconnected && mWasOnline == false) {
        qDebug() << Q_FUNC_INFO << "Account did not go online: " << mAccount->objectPath();
        //setFinishedWithError("PendingRoster Account Error", "Account Did not go onlne");
    }
}
void PendingRosters::onHaveConnectionChanged(bool stat)
{
    Q_UNUSED(stat);
    if (mAccount->haveConnection() ) {
        Tp::ConnectionPtr connection = mAccount->connection();
        connect(connection->becomeReady(Tp::Connection::FeatureRoster),
                SIGNAL(finished(Tp::PendingOperation *)),
                SLOT(onConnectionReady(Tp::PendingOperation *)));
    }
}
void PendingRosters::onConnectionReady(Tp::PendingOperation * op)
{
    if (op->isError()) {
        qWarning() << Q_FUNC_INFO << ": Connection ready failed for account: " << mAccount->objectPath();
        setFinishedWithError(op->errorName(), op->errorMessage());
        return;
    }

    Tp::PendingReady *pr = qobject_cast<Tp::PendingReady *>(op);

    Tp::ConnectionPtr conn = Tp::ConnectionPtr(qobject_cast<Tp::Connection *>(pr->object()));

    if (!mConnectionList.contains(conn)) {
        mConnectionList.append(conn);
        mContacts.unite(conn->contactManager()->allKnownContacts());

            if (mAccount->connection() == conn.data() ) {
                foreach (Tp::ContactPtr contact, mContacts) {
                    QSharedPointer<TpContact> obj =
                        QSharedPointer<TpContact>(new TpContact(contact),
                                                         &QObject::deleteLater);
                    obj->setAccountPath(mAccount->objectPath());
                    mTpContacts.append(obj);
                }

            }
    }
    qDebug() << Q_FUNC_INFO << ": Connection ready for account: " << mAccount->objectPath();

    setFinished();
}
/*!
\fn Tp::Contacts PendingRosters::rosterList()
 \brief Returns the Telepathy rosters of the account when the operation has finished.

 \return Tp::Contacts
*/

Tp::Contacts PendingRosters::rosterList()
{
    return mContacts;
}

/*!

\fn QList<QSharedPointer<TpContact> > PendingRosters::telepathyRosterList()

\brief Returns rosters with extra features when when the operation has finished
\return a list of rosters
\sa PendingRosters::rosterList
*/
QList<QSharedPointer<TpContact> > PendingRosters::telepathyRosterList()
{
    qDebug() << Q_FUNC_INFO << "Found number of rosters for account " << mAccount->objectPath() << ": " << mTpContacts.count();
    return mTpContacts;
}
