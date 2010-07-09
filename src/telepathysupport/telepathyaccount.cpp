#include "telepathyaccount.h"
#include <QtTracker/Tracker>
#include <QtTracker/QLive>
#include <QtTracker/ontologies/nco.h>
#include <TelepathyQt4/Account>
#include <QImage>
using namespace SopranoLive;


class TelepathyAccount::Private
{
    public:
        Private() {}
        ~Private() {}
        Tp::AccountPtr mAccount;
};

QDebug operator<<(QDebug dbg, const Tp::Account &account)
{
    dbg.nospace() << "Tp::Account("
            << "displayName:"  << account.displayName()
            << " nickName:"    << account.nickname()
            //<< " parameters:"  << account.parameters()
            << " uniqueID:"    << account.uniqueIdentifier()
            << ')';
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const Tp::AccountPtr &accountPtr)
{
    return dbg.nospace() << *(accountPtr.constData());
}

QDebug operator<<(QDebug dbg, const TelepathyAccount &account)
{
    return dbg.nospace() << account.account();
}

TelepathyAccount::TelepathyAccount(Tp::AccountPtr account, QObject* parent) : QObject(parent), d(new Private)
{
     d->mAccount = account;
     connect(d->mAccount->becomeReady(Tp::Account::FeatureCore | Tp::Account::FeatureAvatar), SIGNAL(finished(Tp::PendingOperation*)),
             this, SLOT(onAccountReady(Tp::PendingOperation*)));
}


TelepathyAccount::~TelepathyAccount()
{
    delete d;
}

void TelepathyAccount::onAccountReady(Tp::PendingOperation* op)
{
    if (op->isError()) {
        return;
    }

    qDebug() << Q_FUNC_INFO << ": Account became ready: " << d->mAccount->objectPath();

    connect(d->mAccount.data(), SIGNAL(displayNameChanged (const QString &)),
            this,
            SLOT(onDisplayNameChanged(const QString&)));
    connect(d->mAccount.data(), SIGNAL(iconChanged (const QString &)),
                this, SLOT(onIconChanged(const QString&)));
    connect(d->mAccount.data(), SIGNAL (nicknameChanged (const QString &)),
            this, SLOT(onNicknameChanged(const QString&)));
    connect(d->mAccount.data(), SIGNAL(avatarChanged (const Tp::Avatar &)),
            this, SLOT(onAvatarUpdated(const Tp::Avatar&)));
    connect(d->mAccount.data(), SIGNAL(currentPresenceChanged(const Tp::SimplePresence&)),
            this, SLOT(onCurrentPresenceChanged(const Tp::SimplePresence&)));

}

Tp::AccountPtr TelepathyAccount::account() const
{
    return d->mAccount;
}

void TelepathyAccount::onCurrentPresenceChanged(const Tp::SimplePresence& presence)
{
     Q_UNUSED(presence);
     Q_EMIT accountChanged(this, Presence);
}
void TelepathyAccount::onDisplayNameChanged(const QString& name)
{
    Q_UNUSED(name);
    Q_EMIT  accountChanged(this, Alias);
}

void TelepathyAccount::onIconChanged(const QString& icon)
{
    Q_UNUSED(icon);
    Q_EMIT accountChanged(this, Icon);
}
void TelepathyAccount::onNicknameChanged(const QString& nick)
{
    Q_UNUSED(nick);
    Q_EMIT accountChanged(this, NickName);
}

void TelepathyAccount::onAvatarUpdated(const Tp::Avatar& avatar)
{
    Q_UNUSED(avatar);
    Q_EMIT accountChanged(this, Avatar);
}
