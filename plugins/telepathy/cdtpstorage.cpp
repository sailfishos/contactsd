/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#include <tracker-sparql.h>

#include <TelepathyQt4/AvatarData>
#include <TelepathyQt4/ContactCapabilities>
#include <TelepathyQt4/ConnectionCapabilities>
#include <qtcontacts-tracker/phoneutils.h>
#include <ontologies.h>

#include <importstateconst.h>

#include "cdtpstorage.h"
#include "debug.h"

CUBI_USE_NAMESPACE_RESOURCES
using namespace Contactsd;

static const LiteralValue defaultGenerator = LiteralValue(QString::fromLatin1("telepathy"));
static const ResourceValue defaultGraph = ResourceValue(QString::fromLatin1("urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9"));
static const ResourceValue privateGraph = ResourceValue(QString::fromLatin1("urn:uuid:679293d4-60f0-49c7-8d63-f1528fe31f66"));
static const ResourceValue aValue = ResourceValue(QString::fromLatin1("a"), ResourceValue::PrefixedName);
static const Variable imAddressVar = Variable(QString::fromLatin1("imAddress"));
static const Variable imContactVar = Variable(QString::fromLatin1("imContact"));
static const Variable imAccountVar = Variable(QString::fromLatin1("imAccount"));
static const ValueChain imAddressChain = ValueChain() << nco::hasAffiliation::resource() << nco::hasIMAddress::resource();

CDTpStorage::CDTpStorage(QObject *parent) : QObject(parent), mUpdateRunning(false)
{
    mUpdateTimer.setInterval(0);
    mUpdateTimer.setSingleShot(true);
    connect(&mUpdateTimer, SIGNAL(timeout()), SLOT(onUpdateQueueTimeout()));
}

CDTpStorage::~CDTpStorage()
{
}

static void deletePropertyWithGraph(Delete &d, const Value &s, const Value &p, const Variable &g)
{
    Variable o;
    d.addData(s, p, o);

    Graph graph(g);
    graph.addPattern(s, p, o);

    PatternGroup optional;
    optional.setOptional(true);
    optional.addPattern(graph);

    d.addRestriction(optional);
}

static void deleteProperty(Delete &d, const Value &s, const Value &p)
{
    Variable o;
    d.addData(s, p, o);

    PatternGroup optional;
    optional.setOptional(true);
    optional.addPattern(s, p, o);

    d.addRestriction(optional);
}

static ResourceValue presenceType(Tp::ConnectionPresenceType presenceType)
{
    switch (presenceType) {
    case Tp::ConnectionPresenceTypeUnset:
        return nco::presence_status_unknown::resource();
    case Tp::ConnectionPresenceTypeOffline:
        return nco::presence_status_offline::resource();
    case Tp::ConnectionPresenceTypeAvailable:
        return nco::presence_status_available::resource();
    case Tp::ConnectionPresenceTypeAway:
        return nco::presence_status_away::resource();
    case Tp::ConnectionPresenceTypeExtendedAway:
        return nco::presence_status_extended_away::resource();
    case Tp::ConnectionPresenceTypeHidden:
        return nco::presence_status_hidden::resource();
    case Tp::ConnectionPresenceTypeBusy:
        return nco::presence_status_busy::resource();
    case Tp::ConnectionPresenceTypeUnknown:
        return nco::presence_status_unknown::resource();
    case Tp::ConnectionPresenceTypeError:
        return nco::presence_status_error::resource();
    default:
        break;
    }

    warning() << "Unknown telepathy presence status" << presenceType;

    return nco::presence_status_error::resource();
}

static ResourceValue presenceState(Tp::Contact::PresenceState presenceState)
{
    switch (presenceState) {
    case Tp::Contact::PresenceStateNo:
        return nco::predefined_auth_status_no::resource();
    case Tp::Contact::PresenceStateAsk:
        return nco::predefined_auth_status_requested::resource();
    case Tp::Contact::PresenceStateYes:
        return nco::predefined_auth_status_yes::resource();
    }

    warning() << "Unknown telepathy presence state:" << presenceState;

    return nco::predefined_auth_status_no::resource();
}

static LiteralValue literalTimeStamp()
{
    return LiteralValue(QDateTime::currentDateTime());
}

static QString imAddress(const QString &accountPath, const QString &contactId)
{
    static const QString tmpl = QString::fromLatin1("telepathy:%1!%2");
    return tmpl.arg(accountPath, contactId);
}

static QString imAddress(const QString &accountPath)
{
    static const QString tmpl = QString::fromLatin1("telepathy:%1!self");
    return tmpl.arg(accountPath);
}

static QString imAccount(const QString &accountPath)
{
    static const QString tmpl = QString::fromLatin1("telepathy:%1");
    return tmpl.arg(accountPath);
}

static ResourceValue literalIMAddress(const QString &accountPath, const QString &contactId)
{
    return ResourceValue(imAddress(accountPath, contactId));
}

static ResourceValue literalIMAddress(const CDTpContactPtr &contactWrapper)
{
    const QString accountPath = contactWrapper->accountWrapper()->account()->objectPath();
    const QString contactId = contactWrapper->contact()->id();
    return ResourceValue(imAddress(accountPath, contactId));
}

static ResourceValue literalIMAddress(const CDTpAccountPtr &accountWrapper)
{
    return ResourceValue(imAddress(accountWrapper->account()->objectPath()));
}

static ValueList literalIMAddressList(const QList<CDTpContactPtr> &contacts)
{
    ValueList list;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        const QString accountPath = contactWrapper->accountWrapper()->account()->objectPath();
        const QString contactId = contactWrapper->contact()->id();
        list.addValue(LiteralValue(imAddress(accountPath, contactId)));
    }
    return list;
}

static ValueList literalIMAddressList(const QList<CDTpAccountPtr> &accounts)
{
    ValueList list;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        list.addValue(LiteralValue(imAddress(accountWrapper->account()->objectPath())));
    }
    return list;
}

static ResourceValue literalIMAccount(const QString &accountPath)
{
    return ResourceValue(imAccount(accountPath));
}

static ResourceValue literalIMAccount(const CDTpAccountPtr &accountWrapper)
{
    return ResourceValue(imAccount(accountWrapper->account()->objectPath()));
}

static ValueList literalIMAccountList(const QList<CDTpAccountPtr> &accounts)
{
    ValueList list;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        list.addValue(LiteralValue(imAccount(accountWrapper->account()->objectPath())));
    }
    return list;
}

static LiteralValue literalContactInfo(const Tp::ContactInfoField &field, int i)
{
    if (i >= field.fieldValue.count()) {
        return LiteralValue(QLatin1String(""));
    }

    return LiteralValue(field.fieldValue[i]);
}

static void addPresence(PatternGroup &g,
        const Value &imAddress,
        const Tp::Presence &presence)
{
    g.addPattern(imAddress, nco::imPresence::resource(), presenceType(presence.type()));
    g.addPattern(imAddress, nco::presenceLastModified::resource(), literalTimeStamp());
    g.addPattern(imAddress, nco::imStatusMessage::resource(), LiteralValue(presence.statusMessage()));
}

static void addCapabilities(PatternGroup &g,
        const Value &imAddress,
        Tp::CapabilitiesBase capabilities)
{
    /* FIXME: We could also add im_capability_stream_tubes and
     * im_capability_dbus_tubes */

    if (capabilities.textChats()) {
        g.addPattern(imAddress, nco::imCapability::resource(), nco::im_capability_text_chat::resource());
    }
    if (capabilities.streamedMediaCalls()) {
        g.addPattern(imAddress, nco::imCapability::resource(), nco::im_capability_media_calls::resource());
    }
    if (capabilities.streamedMediaAudioCalls()) {
        g.addPattern(imAddress, nco::imCapability::resource(), nco::im_capability_audio_calls::resource());
    }
    if (capabilities.streamedMediaVideoCalls()) {
        g.addPattern(imAddress, nco::imCapability::resource(), nco::im_capability_video_calls::resource());
    }
    if (capabilities.upgradingStreamedMediaCalls()) {
        g.addPattern(imAddress, nco::imCapability::resource(), nco::im_capability_upgrading_calls::resource());
    }
    if (capabilities.fileTransfers()) {
        g.addPattern(imAddress, nco::imCapability::resource(), nco::im_capability_file_transfers::resource());
    }
}

static void addAvatar(PatternGroup &g,
        const Value &imAddress,
        const QString &fileName)
{
    if (!fileName.isEmpty()) {
        const QUrl url = QUrl::fromLocalFile(fileName);
        const Value dataObject = ResourceValue(url);
        g.addPattern(dataObject, aValue, nfo::FileDataObject::resource());
        g.addPattern(dataObject, nie::url::resource(), LiteralValue(url));
        g.addPattern(imAddress, nco::imAvatar::resource(), dataObject);
    }
}

static Value ensureContactAffiliation(PatternGroup &g,
        QHash<QString, Value> &affiliations,
        QString affiliationLabel,
        const Value &imContact)
{
    if (!affiliations.contains(affiliationLabel)) {
        const BlankValue affiliation;
        g.addPattern(affiliation, aValue, nco::Affiliation::resource());
        g.addPattern(affiliation, rdfs::label::resource(), LiteralValue(affiliationLabel));
        g.addPattern(imContact, nco::hasAffiliation::resource(), affiliation);
        affiliations.insert(affiliationLabel, affiliation);
    }

    return affiliations[affiliationLabel];
}

static CDTpQueryBuilder createContactInfoBuilder(CDTpContactPtr contactWrapper)
{
    if (!contactWrapper->isInformationKnown()) {
        debug() << "contact information is unknown";
        return CDTpQueryBuilder();
    }

    Tp::ContactInfoFieldList listContactInfo = contactWrapper->contact()->infoFields().allFields();
    if (listContactInfo.count() == 0) {
        debug() << "contact information is empty";
        return CDTpQueryBuilder();
    }

    /* Create a builder with ?imContact bound to this contact.
     * Use the imAddress as graph for ContactInfo fields, so we can easilly
     * know from which contact it comes from */
    CDTpQueryBuilder builder;
    Insert i(Insert::Replace);
    Graph g(literalIMAddress(contactWrapper));
    i.addRestriction(imContactVar, imAddressChain, literalIMAddress(contactWrapper));

    QHash<QString, Value> affiliations;
    Q_FOREACH (const Tp::ContactInfoField &field, listContactInfo) {
        if (field.fieldValue.count() == 0) {
            continue;
        }

        /* Extract field types */
        QStringList subTypes;
        QString affiliationLabel = QLatin1String("Other");
        Q_FOREACH (const QString &param, field.parameters) {
            if (!param.startsWith(QLatin1String("type="))) {
                continue;
            }
            const QString type = param.mid(5);
            if (type == QLatin1String("home")) {
                affiliationLabel = QLatin1String("Home");
            } else if (type == QLatin1String("work")) {
                affiliationLabel = QLatin1String("Work");
            } else if (!subTypes.contains(type)){
                subTypes << type;
            }
        }

        /* FIXME: do we care about "fn" and "nickname" ? */
        if (!field.fieldName.compare(QLatin1String("tel"))) {
            static QHash<QString, Value> knownTypes;
            if (knownTypes.isEmpty()) {
                knownTypes.insert(QLatin1String("bbsl"), nco::BbsNumber::resource());
                knownTypes.insert(QLatin1String("car"), nco::CarPhoneNumber::resource());
                knownTypes.insert(QLatin1String("cell"), nco::CellPhoneNumber::resource());
                knownTypes.insert(QLatin1String("fax"), nco::FaxNumber::resource());
                knownTypes.insert(QLatin1String("isdn"), nco::IsdnNumber::resource());
                knownTypes.insert(QLatin1String("modem"), nco::ModemNumber::resource());
                knownTypes.insert(QLatin1String("pager"), nco::PagerNumber::resource());
                knownTypes.insert(QLatin1String("pcs"), nco::PcsNumber::resource());
                knownTypes.insert(QLatin1String("video"), nco::VideoTelephoneNumber::resource());
                knownTypes.insert(QLatin1String("voice"), nco::VoicePhoneNumber::resource());
            }

            QStringList realSubTypes;
            Q_FOREACH (const QString &type, subTypes) {
                if (knownTypes.contains(type)) {
                    realSubTypes << type;
                }
            }

            const Value affiliation = ensureContactAffiliation(g, affiliations, affiliationLabel, imContactVar);
            const Value phoneNumber = qctMakePhoneNumberResource(field.fieldValue[0], realSubTypes);
            g.addPattern(phoneNumber, aValue, nco::PhoneNumber::resource());
            Q_FOREACH (const QString &type, realSubTypes) {
                g.addPattern(phoneNumber, aValue, knownTypes[type]);
            }
            g.addPattern(phoneNumber, nco::phoneNumber::resource(), literalContactInfo(field, 0));
            g.addPattern(phoneNumber, maemo::localPhoneNumber::resource(),
                    LiteralValue(qctMakeLocalPhoneNumber(field.fieldValue[0])));
            g.addPattern(affiliation, nco::hasPhoneNumber::resource(), phoneNumber);
        }

        else if (!field.fieldName.compare(QLatin1String("adr"))) {
            static QHash<QString, Value> knownTypes;
            if (knownTypes.isEmpty()) {
                knownTypes.insert(QLatin1String("dom"), nco::DomesticDeliveryAddress::resource());
                knownTypes.insert(QLatin1String("intl"), nco::InternationalDeliveryAddress::resource());
                knownTypes.insert(QLatin1String("parcel"), nco::ParcelDeliveryAddress::resource());
                knownTypes.insert(QLatin1String("postal"), maemo::PostalAddress::resource());
            }

            QStringList realSubTypes;
            Q_FOREACH (const QString &type, subTypes) {
                if (knownTypes.contains(type)) {
                    realSubTypes << type;
                }
            }

            const Value affiliation = ensureContactAffiliation(g, affiliations, affiliationLabel, imContactVar);
            const BlankValue postalAddress;
            g.addPattern(postalAddress, aValue, nco::PostalAddress::resource());
            Q_FOREACH (const QString &type, realSubTypes) {
                g.addPattern(postalAddress, aValue, knownTypes[type]);
            }
            g.addPattern(postalAddress, nco::pobox::resource(),           literalContactInfo(field, 0));
            g.addPattern(postalAddress, nco::extendedAddress::resource(), literalContactInfo(field, 1));
            g.addPattern(postalAddress, nco::streetAddress::resource(),   literalContactInfo(field, 2));
            g.addPattern(postalAddress, nco::locality::resource(),        literalContactInfo(field, 3));
            g.addPattern(postalAddress, nco::region::resource(),          literalContactInfo(field, 4));
            g.addPattern(postalAddress, nco::postalcode::resource(),      literalContactInfo(field, 5));
            g.addPattern(postalAddress, nco::country::resource(),         literalContactInfo(field, 6));
            g.addPattern(affiliation, nco::hasPostalAddress::resource(), postalAddress);
        }

        else if (!field.fieldName.compare(QLatin1String("email"))) {
            static const QString tmpl = QString::fromLatin1("mailto:%1");
            const Value emailAddress = ResourceValue(tmpl.arg(field.fieldValue[0]));
            const Value affiliation = ensureContactAffiliation(g, affiliations, affiliationLabel, imContactVar);
            g.addPattern(emailAddress, aValue, nco::EmailAddress::resource());
            g.addPattern(emailAddress, nco::emailAddress::resource(), literalContactInfo(field, 0));
            g.addPattern(affiliation, nco::hasEmailAddress::resource(), emailAddress);
        }

        else if (!field.fieldName.compare(QLatin1String("url"))) {
            const Value affiliation = ensureContactAffiliation(g, affiliations, affiliationLabel, imContactVar);
            g.addPattern(affiliation, nco::url::resource(), literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("title"))) {
            const Value affiliation = ensureContactAffiliation(g, affiliations, affiliationLabel, imContactVar);
            g.addPattern(affiliation, nco::title::resource(), literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("role"))) {
            const Value affiliation = ensureContactAffiliation(g, affiliations, affiliationLabel, imContactVar);
            g.addPattern(affiliation, nco::role::resource(), literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("org"))) {
            const Value affiliation = ensureContactAffiliation(g, affiliations, affiliationLabel, imContactVar);
            const BlankValue organizationContact;
            g.addPattern(organizationContact, aValue, nco::OrganizationContact::resource());
            g.addPattern(organizationContact, nco::fullname::resource(), literalContactInfo(field, 0));
            g.addPattern(affiliation, nco::department::resource(), literalContactInfo(field, 1));
            g.addPattern(affiliation, nco::org::resource(), organizationContact);
        }

        else if (!field.fieldName.compare(QLatin1String("note")) || !field.fieldName.compare(QLatin1String("desc"))) {
            g.addPattern(imContactVar, nco::note::resource(), literalContactInfo(field, 0));
        }

        else if (!field.fieldName.compare(QLatin1String("bday"))) {
            /* Tracker will reject anything not [-]CCYY-MM-DDThh:mm:ss[Z|(+|-)hh:mm]
             * VCard spec allows only ISO 8601, but most IM clients allows
             * any string. */
            /* FIXME: support more date format for compatibility */
            QDate date = QDate::fromString(field.fieldValue[0], QLatin1String("yyyy-MM-dd"));
            if (!date.isValid()) {
                date = QDate::fromString(field.fieldValue[0], QLatin1String("yyyyMMdd"));
            }

            if (date.isValid()) {
                g.addPattern(imContactVar, nco::birthDate::resource(), LiteralValue(QDateTime(date)));
            } else {
                debug() << "Unsupported bday format:" << field.fieldValue[0];
            }
        }

        else if (!field.fieldName.compare(QLatin1String("x-gender"))) {
            if (field.fieldValue[0] == QLatin1String("male")) {
                g.addPattern(imContactVar, nco::gender::resource(), nco::gender_male::resource());
            } else if (field.fieldValue[0] == QLatin1String("female")) {
                g.addPattern(imContactVar, nco::gender::resource(), nco::gender_female::resource());
            } else {
                debug() << "Unsupported gender:" << field.fieldValue[0];
            }
        }

        else {
            debug() << "Unsupported VCard field" << field.fieldName;
        }
    }
    i.addData(g);
    builder.append(i);

    return builder;
}

static void addRemoveContactInfo(Delete &d,
        const Variable &imAddress,
        const Value &imContact)
{
    /* Remove all triples on imContact and in graph. All sub-resources will be
     * GCed by qct sometimes.
     * imAddress is used as graph for properties on the imContact */
    deletePropertyWithGraph(d, imContact, nco::birthDate::resource(), imAddress);
    deletePropertyWithGraph(d, imContact, nco::gender::resource(), imAddress);
    deletePropertyWithGraph(d, imContact, nco::note::resource(), imAddress);
    deletePropertyWithGraph(d, imContact, nco::hasAffiliation::resource(), imAddress);
}

static QString saveAccountAvatar(CDTpAccountPtr accountWrapper)
{
    const Tp::Avatar &avatar = accountWrapper->account()->avatar();

    if (avatar.avatarData.isEmpty()) {
        return QString();
    }

    static const QString tmpl = QString::fromLatin1("%1/.contacts/avatars/%2");
    QString fileName = tmpl.arg(QDir::homePath())
        .arg(QLatin1String(QCryptographicHash::hash(avatar.avatarData, QCryptographicHash::Sha1).toHex()));
    debug() << "Saving account avatar to" << fileName;

    QFile avatarFile(fileName);
    if (!avatarFile.open(QIODevice::WriteOnly)) {
        warning() << "Unable to save account avatar: error opening avatar "
            "file" << fileName << "for writing";
        return QString();
    }
    avatarFile.write(avatar.avatarData);
    avatarFile.close();

    return fileName;
}

static void addAccountChanges(PatternGroup &g,
        CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes)
{
    Tp::AccountPtr account = accountWrapper->account();
    const Value imAccount = literalIMAccount(accountWrapper);
    const Value imAddress = literalIMAddress(accountWrapper);

    if (changes & CDTpAccount::Presence) {
        debug() << "  presence changed";
        addPresence(g, imAddress, account->currentPresence());
    }
    if (changes & CDTpAccount::Avatar) {
        debug() << "  avatar changed";
        addAvatar(g, imAddress, saveAccountAvatar(accountWrapper));
    }
    if (changes & CDTpAccount::Nickname) {
        debug() << "  nickname changed";
        g.addPattern(imAddress, nco::imNickname::resource(), LiteralValue(account->nickname()));
    }
    if (changes & CDTpAccount::DisplayName) {
        debug() << "  display name changed";
        g.addPattern(imAccount, nco::imDisplayName::resource(), LiteralValue(account->displayName()));
    }
    if (changes & CDTpAccount::Enabled) {
        debug() << "  enabled changed";
        g.addPattern(imAccount, nco::imEnabled::resource(), LiteralValue(account->isEnabled()));
    }
}

static void addContactChanges(PatternGroup &g,
        const Value &imAddress,
        CDTpContact::Changes changes,
        Tp::ContactPtr contact)
{
    debug() << "Update contact" << imAddress.sparql();

    // Apply changes
    if (changes & CDTpContact::Alias) {
        debug() << "  alias changed";
        g.addPattern(imAddress, nco::imNickname::resource(), LiteralValue(contact->alias().trimmed()));
    }
    if (changes & CDTpContact::Presence) {
        debug() << "  presence changed";
        addPresence(g, imAddress, contact->presence());
    }
    if (changes & CDTpContact::Capabilities) {
        debug() << "  capabilities changed";
        addCapabilities(g, imAddress, contact->capabilities());
    }
    if (changes & CDTpContact::Avatar) {
        debug() << "  avatar changed";
        addAvatar(g, imAddress, contact->avatarData().fileName);
    }
    if (changes & CDTpContact::Authorization) {
        debug() << "  authorization changed";
        g.addPattern(imAddress, nco::imAddressAuthStatusFrom::resource(),
                presenceState(contact->subscriptionState()));
        g.addPattern(imAddress, nco::imAddressAuthStatusTo::resource(),
                presenceState(contact->publishState()));
    }
}

static CDTpQueryBuilder createAccountsBuilder(const QList<CDTpAccountPtr> &accounts)
{
    CDTpQueryBuilder builder;
    Insert i(Insert::Replace);

    Graph g(defaultGraph);
    g.addPattern(nco::default_contact_me::resource(), nie::contentLastModified::resource(), literalTimeStamp());
    i.addData(g);

    g = Graph(privateGraph);
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        Tp::AccountPtr account = accountWrapper->account();
        const Value imAccount = literalIMAccount(accountWrapper);
        const Value imAddress = literalIMAddress(accountWrapper);

        debug() << "Create account" << imAddress.sparql();

        // Ensure the IMAccount exists
        g.addPattern(imAccount, aValue, nco::IMAccount::resource());
        g.addPattern(imAccount, nco::imAccountType::resource(), LiteralValue(account->protocolName()));
        g.addPattern(imAccount, nco::imAccountAddress::resource(), imAddress);
        g.addPattern(imAccount, nco::hasIMContact::resource(), imAddress);

        // Ensure the self contact has an IMAddress
        g.addPattern(imAddress, aValue, nco::IMAddress::resource());
        g.addPattern(imAddress, nco::imID::resource(), LiteralValue(account->normalizedName()));
        g.addPattern(imAddress, nco::imProtocol::resource(), LiteralValue(account->protocolName()));

        // Add all mutable properties
        addAccountChanges(g, accountWrapper, CDTpAccount::All);
    }
    i.addData(g);
    builder.append(i);

    // Ensure the IMAddresses are bound to the default-contact-me via an affiliation
    const Value affiliation = BlankValue(QString::fromLatin1("affiliation"));
    Exists e;
    i = Insert(Insert::Replace);
    g = Graph(privateGraph);
    g.addPattern(nco::default_contact_me::resource(), nco::hasAffiliation::resource(), affiliation);
    g.addPattern(affiliation, aValue, nco::Affiliation::resource());
    g.addPattern(affiliation, nco::hasIMAddress::resource(), imAddressVar);
    g.addPattern(affiliation, rdfs::label::resource(), LiteralValue(QString::fromLatin1("Other")));
    i.addData(g);
    i.addRestriction(imAddressVar, aValue, nco::IMAddress::resource());
    e.addPattern(nco::default_contact_me::resource(), imAddressChain, imAddressVar);
    i.setFilter(Filter(Functions::and_.apply(
            Functions::in.apply(Functions::str.apply(imAddressVar), literalIMAddressList(accounts)),
            Functions::not_.apply(Filter(e)))));
    builder.append(i);

    return builder;
}

static CDTpQueryBuilder createContactsBuilder(const QList<CDTpContactPtr> &contacts)
{
    CDTpQueryBuilder builder;

    // Ensure all imAddress exists and are linked from imAccount
    Insert i(Insert::Replace);
    Graph g(privateGraph);
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        CDTpAccountPtr accountWrapper = contactWrapper->accountWrapper();

        const Value imAddress = literalIMAddress(contactWrapper);
        g.addPattern(imAddress, aValue, nco::IMAddress::resource());
        g.addPattern(imAddress, nco::imID::resource(), LiteralValue(contactWrapper->contact()->id()));
        g.addPattern(imAddress, nco::imProtocol::resource(), LiteralValue(accountWrapper->account()->protocolName()));

        const Value imAccount = literalIMAccount(accountWrapper);
        g.addPattern(imAccount, nco::hasIMContact::resource(), imAddress);

        // Add mutable properties except for ContactInfo
        addContactChanges(g, imAddress, CDTpContact::All, contactWrapper->contact());
    }
    i.addData(g);
    builder.append(i);

    // Ensure all imAddresses are bound to a PersonContact
    i = Insert();
    g = Graph(defaultGraph);
    BlankValue imContact(QString::fromLatin1("contact"));
    g.addPattern(imContact, aValue, nco::PersonContact::resource());
    g.addPattern(imContact, nie::contentCreated::resource(), literalTimeStamp());
    g.addPattern(imContact, nie::contentLastModified::resource(), literalTimeStamp());
    g.addPattern(imContact, nie::generator::resource(), defaultGenerator);
    i.addData(g);
    g = Graph(privateGraph);
    BlankValue affiliation(QString::fromLatin1("affiliation"));
    g.addPattern(imContact, nco::hasAffiliation::resource(), affiliation);
    g.addPattern(affiliation, aValue, nco::Affiliation::resource());
    g.addPattern(affiliation, nco::hasIMAddress::resource(), imAddressVar);
    g.addPattern(affiliation, rdfs::label::resource(), LiteralValue(QString::fromLatin1("Other")));
    i.addData(g);
    g = Graph(privateGraph);
    Variable imId;
    g.addPattern(imAddressVar, nco::imID::resource(), imId);
    i.addRestriction(g);
    Exists e;
    e.addPattern(imContactVar, imAddressChain, imAddressVar);
    i.setFilter(Functions::not_.apply(Filter(e)));
    builder.append(i);

    // Add ContactInfo seperately because we need to bind to the PersonContact
    Q_FOREACH (const CDTpContactPtr contactWrapper, contacts) {
        builder.append(createContactInfoBuilder(contactWrapper));
    }

    return builder;
}

static CDTpQueryBuilder purgeContactsBuilder()
{
    /* Purge nco:IMAddress not bound from an nco:IMAccount */

    CDTpQueryBuilder builder;

    /* Step 1 - Clean nco:PersonContact from all info imported from the imAddress.
     * Note: We don't delete the affiliation because it could contain other
     * info (See NB#239973) */
    Delete d;
    Variable affiliationVar;
    d.addData(affiliationVar, nco::hasIMAddress::resource(), imAddressVar);
    Graph g(privateGraph);
    Variable imId;
    g.addPattern(imAddressVar, nco::imID::resource(), imId);
    d.addRestriction(g);
    d.addRestriction(imContactVar, nco::hasAffiliation::resource(), affiliationVar);
    d.addRestriction(affiliationVar, nco::hasIMAddress::resource(), imAddressVar);
    Exists e;
    e.addPattern(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.setFilter(Filter(Functions::not_.apply(Filter(e))));
    deleteProperty(d, imContactVar, nie::contentLastModified::resource());
    addRemoveContactInfo(d, imAddressVar, imContactVar);
    builder.append(d);

    /* Step 1.1 - Remove the nco:IMAddress resource.
     * This must be done in a separate query because of NB#242979 */
    d = Delete();
    d.addData(imAddressVar, aValue, rdfs::Resource::resource());
    g = Graph(privateGraph);
    imId = Variable();
    g.addPattern(imAddressVar, nco::imID::resource(), imId);
    d.addRestriction(g);
    e = Exists();
    e.addPattern(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.setFilter(Filter(Functions::not_.apply(Filter(e))));
    builder.append(d);

    /* Step 2 - Purge nco:PersonContact with generator "telepathy" but with no
     * nco:IMAddress bound anymore */
    d = Delete();
    d.addData(imContactVar, aValue, rdfs::Resource::resource());
    d.addRestriction(imContactVar, nie::generator::resource(), defaultGenerator);
    e = Exists();
    Variable v;
    e.addPattern(imContactVar, imAddressChain, v);
    d.setFilter(Filter(Functions::not_.apply(Filter(e))));
    builder.append(d);

    /* Step 3 - Add back nie:contentLastModified for nco:PersonContact missing one */
    Insert i(Insert::Replace);
    g = Graph(defaultGraph);
    g.addPattern(imContactVar, nie::contentLastModified::resource(), literalTimeStamp());
    i.addData(g);
    i.addRestriction(imContactVar, aValue, nco::PersonContact::resource());
    e = Exists();
    v = Variable();
    e.addPattern(imContactVar, nie::contentLastModified::resource(), v);
    i.setFilter(Filter(Functions::not_.apply(Filter(e))));
    builder.append(i);

    return builder;
}

static Tp::Presence unknownPresence()
{
    static const Tp::Presence unknownPresence(Tp::ConnectionPresenceTypeUnknown, QLatin1String("unknown"), QString());
    return unknownPresence;
}

static CDTpQueryBuilder syncNoRosterAccountsContactsBuilder(const QList<CDTpAccountPtr> accounts)
{
    CDTpQueryBuilder builder;

    // Set presence to UNKNOWN for all contacts, except for self contact because
    // its presence is OFFLINE and will be set in updateAccount()
    Insert i(Insert::Replace);
    Graph g(privateGraph);
    addPresence(g, imAddressVar, unknownPresence());
    i.addData(g);
    g = Graph(defaultGraph);
    g.addPattern(imContactVar, nie::contentLastModified::resource(), literalTimeStamp());
    i.addData(g);
    i.addRestriction(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    i.addRestriction(imContactVar, imAddressChain, imAddressVar);
    i.setFilter(Filter(Functions::and_.apply(
            Functions::notIn.apply(Functions::str.apply(imAddressVar), literalIMAddressList(accounts)),
            Functions::in.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts)))));
    builder.append(i);

    // Remove capabilities from all imAddresses
    Delete d;
    d.addRestriction(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts))));
    deleteProperty(d, imAddressVar, nco::imCapability::resource());
    builder.append(d);

    // Add capabilities on all contacts for each account
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        Insert i;
        Graph g(privateGraph);
        addCapabilities(g, imAddressVar, accountWrapper->account()->capabilities());
        i.addData(g);
        i.addRestriction(literalIMAccount(accountWrapper), nco::hasIMContact::resource(), imAddressVar);
        builder.append(i);
    }

    return builder;
}

static CDTpQueryBuilder syncRosterAccountsContactsBuilder(const QList<CDTpAccountPtr> &accounts,
        bool purgeContacts = false)
{
    QList<CDTpContactPtr> allContacts;
    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        allContacts << accountWrapper->contacts();
    }

    /* Delete the hasIMContact property on IMAccounts (except for self contact)
     * then sync all contacts (that will add back hasIMContact for them). After
     * that we can purge all imAddresses not bound to an IMAccount anymore.
     *
     * Also remove imCapability from imAddresses because it is multi-valued */
    CDTpQueryBuilder builder;
    Delete d;
    d.addData(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.addRestriction(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.setFilter(Filter(Functions::and_.apply(
            Functions::notIn.apply(Functions::str.apply(imAddressVar), literalIMAddressList(accounts)),
            Functions::in.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts)))));
    deleteProperty(d, imAddressVar, nco::imCapability::resource());
    builder.append(d);

    /* Delete ContactInfo for those who know it */
    QList<CDTpContactPtr> infoContacts;
    Q_FOREACH (const CDTpContactPtr contactWrapper, allContacts) {
        if (contactWrapper->isInformationKnown()) {
            infoContacts << contactWrapper;
        }
    }
    if (!infoContacts.isEmpty()) {
        Delete d;
        d.addRestriction(imContactVar, imAddressChain, imAddressVar);
        d.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAddressVar), literalIMAddressList(infoContacts))));
        addRemoveContactInfo(d, imAddressVar, imContactVar);
        builder.append(d);
    }

    /* Now create all contacts */
    if (!allContacts.isEmpty()) {
        builder.append(createContactsBuilder(allContacts));

        /* Update timestamp on all nco:PersonContact bound to this account */
        Insert i(Insert::Replace);
        Graph g(defaultGraph);
        g.addPattern(imContactVar, nie::contentLastModified::resource(), literalTimeStamp());
        i.addData(g);
        i.addRestriction(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
        i.addRestriction(imContactVar, imAddressChain, imAddressVar);
        i.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts))));
        builder.append(i);
    }

    /* Purge IMAddresses not bound from an account anymore */
    if (purgeContacts) {
        builder.append(purgeContactsBuilder());
    }

    return builder;
}

static CDTpQueryBuilder syncDisabledAccountsContactsBuilder(const QList<CDTpAccountPtr> &accounts)
{
    CDTpQueryBuilder builder;

    /* Disabled account: We want remove pure-IM contacts, but for merged/edited
     * contacts we want to keep only its IMAddress as local information, so it
     * will be merged again when we re-enable the account. */

    /* Step 1 - Unlink pure-IM IMAddress from its IMAccount */
    Delete d;
    d.addData(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.addRestriction(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.addRestriction(imContactVar, imAddressChain, imAddressVar);
    d.addRestriction(imContactVar, nie::generator::resource(), defaultGenerator);
    d.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts))));
    builder.append(d);

    /* Step 2 - Delete pure-IM contacts since they are now unbound */
    builder.append(purgeContactsBuilder());

    /* Step 3 - remove all imported info from merged/edited contacts (those remaining) */
    d = Delete();
    addRemoveContactInfo(d, imAddressVar, imContactVar);
    deleteProperty(d, imAddressVar, nco::imPresence::resource());
    deleteProperty(d, imAddressVar, nco::presenceLastModified::resource());
    deleteProperty(d, imAddressVar, nco::imStatusMessage::resource());
    deleteProperty(d, imAddressVar, nco::imCapability::resource());
    deleteProperty(d, imAddressVar, nco::imAvatar::resource());
    deleteProperty(d, imAddressVar, nco::imNickname::resource());
    deleteProperty(d, imAddressVar, nco::imAddressAuthStatusFrom::resource());
    deleteProperty(d, imAddressVar, nco::imAddressAuthStatusTo::resource());
    d.addRestriction(imContactVar, imAddressChain, imAddressVar);
    d.addRestriction(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    d.setFilter(Filter(Functions::and_.apply(
        Functions::in.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts)),
        Functions::notIn.apply(Functions::str.apply(imAddressVar), literalIMAddressList(accounts)))));
    builder.append(d);

    /* Step 4 - move merged/edited IMAddress to qct's graph and update timestamp */
    Insert i(Insert::Replace);
    Graph g(defaultGraph);
    Variable imId;
    g.addPattern(imContactVar, nie::contentLastModified::resource(), literalTimeStamp());
    g.addPattern(imAddressVar, nco::imID::resource(), imId);
    i.addData(g);
    g = Graph(privateGraph);
    g.addPattern(imAddressVar, nco::imID::resource(), imId);
    i.addRestriction(g);
    i.addRestriction(imAccountVar, nco::hasIMContact::resource(), imAddressVar);
    i.addRestriction(imContactVar, imAddressChain, imAddressVar);
    i.setFilter(Filter(Functions::and_.apply(
        Functions::in.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts)),
        Functions::notIn.apply(Functions::str.apply(imAddressVar), literalIMAddressList(accounts)))));
    builder.append(i);

    return builder;
}

static CDTpQueryBuilder removeContactsBuilder(const QString &accountPath,
        const QStringList &contactIds)
{
    // delete all nco:hasIMContact link from the nco:IMAccount
    CDTpQueryBuilder builder;
    Delete d;
    const Value imAccount = literalIMAccount(accountPath);
    Q_FOREACH (const QString &contactId, contactIds) {
        d.addData(imAccount, nco::hasIMContact::resource(), literalIMAddress(accountPath, contactId));
    }
    builder.append(d);

    // Now purge contacts not linked from an IMAccount
    builder.append(purgeContactsBuilder());

    return builder;
}

static CDTpQueryBuilder removeContactsBuilder(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contacts)
{
    QStringList contactIds;
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        contactIds << contactWrapper->contact()->id();
    }

    return removeContactsBuilder(accountWrapper->account()->objectPath(), contactIds);
}

void CDTpStorage::syncAccounts(const QList<CDTpAccountPtr> &accounts)
{
    /* Remove IMAccount that does not exist anymore */
    CDTpQueryBuilder builder;
    Delete d;
    d.addData(imAccountVar, aValue, rdfs::Resource::resource());
    d.addRestriction(imAccountVar, aValue, nco::IMAccount::resource());
    if (!accounts.isEmpty()) {
        d.setFilter(Filter(Functions::notIn.apply(Functions::str.apply(imAccountVar), literalIMAccountList(accounts))));
    }
    builder.append(d);

    /* Sync accounts and their contacts */
    if (!accounts.isEmpty()) {
        builder.append(createAccountsBuilder(accounts));

        // Split accounts that have their roster, and those who don't
        QList<CDTpAccountPtr> disabledAccounts;
        QList<CDTpAccountPtr> rosterAccounts;
        QList<CDTpAccountPtr> noRosterAccounts;
        Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
            if (!accountWrapper->isEnabled()) {
                disabledAccounts << accountWrapper;
            } else if (accountWrapper->hasRoster()) {
                rosterAccounts << accountWrapper;
            } else {
                noRosterAccounts << accountWrapper;
            }
        }

        // Sync contacts
        if (!disabledAccounts.isEmpty()) {
            builder.append(syncDisabledAccountsContactsBuilder(disabledAccounts));
        }

        if (!rosterAccounts.isEmpty()) {
            builder.append(syncRosterAccountsContactsBuilder(rosterAccounts));
        }

        if (!noRosterAccounts.isEmpty()) {
            builder.append(syncNoRosterAccountsContactsBuilder(noRosterAccounts));
        }
    }

    /* Purge IMAddresses not bound from an account anymore, this include the
     * self IMAddress and the default-contact-me as well */
    builder.append(purgeContactsBuilder());

    CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSparqlQueryFinished(CDTpSparqlQuery *)));
}

void CDTpStorage::createAccount(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder;

    /* Create account */
    QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
    builder.append(createAccountsBuilder(accounts));

    /* if account has no contacts, we are done */
    if (accountWrapper->contacts().isEmpty()) {
        CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
        connect(query,
                SIGNAL(finished(CDTpSparqlQuery *)),
                SLOT(onSparqlQueryFinished(CDTpSparqlQuery *)));
        return;
    }

    /* Create account's contacts */
    builder.append(createContactsBuilder(accountWrapper->contacts()));

    /* Update timestamp on all nco:PersonContact bound to this account */
    Insert i(Insert::Replace);
    Graph g(defaultGraph);
    g.addPattern(imContactVar, nie::contentLastModified::resource(), literalTimeStamp());
    i.addData(g);
    i.addRestriction(literalIMAccount(accountWrapper), nco::hasIMContact::resource(), imAddressVar);
    i.addRestriction(imContactVar, imAddressChain, imAddressVar);
    builder.append(i);

    /* Notify import progress */
    Q_EMIT syncStarted(accountWrapper);
    CDTpAccountsSparqlQuery *query = new CDTpAccountsSparqlQuery(accountWrapper, builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSyncOperationEnded(CDTpSparqlQuery *)));
}

void CDTpStorage::updateAccount(CDTpAccountPtr accountWrapper,
        CDTpAccount::Changes changes)
{
    debug() << "Update account" << literalIMAddress(accountWrapper).sparql();

    CDTpQueryBuilder builder;

    Insert i(Insert::Replace);
    Graph g(defaultGraph);
    g.addPattern(nco::default_contact_me::resource(), nie::contentLastModified::resource(), literalTimeStamp());
    i.addData(g);

    g = Graph(privateGraph);
    addAccountChanges(g, accountWrapper, changes);
    i.addData(g);

    builder.append(i);

    /* If account got disabled, we have special threatment for its contacts */
    if ((changes & CDTpAccount::Enabled) != 0 && !accountWrapper->isEnabled()) {
        cancelQueuedUpdates(accountWrapper->contacts());
        QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
        builder.append(syncDisabledAccountsContactsBuilder(accounts));
    }

    CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSparqlQueryFinished(CDTpSparqlQuery *)));
}

void CDTpStorage::removeAccount(CDTpAccountPtr accountWrapper)
{
    cancelQueuedUpdates(accountWrapper->contacts());

    const Value imAccount = literalIMAccount(accountWrapper);
    debug() << "Remove account" << imAccount.sparql();

    /* Remove account */
    CDTpQueryBuilder builder;
    Delete d;
    d.addData(imAccount, aValue, rdfs::Resource::resource());
    builder.append(d);

    /* Purge IMAddresses not bound from an account anymore.
     * This will at least remove the IMAddress of the self contact and update
     * default-contact-me */
    builder.append(purgeContactsBuilder());

    CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSparqlQueryFinished(CDTpSparqlQuery *)));
}

// This is called when account goes online/offline
void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper)
{
    CDTpQueryBuilder builder;

    QList<CDTpAccountPtr> accounts = QList<CDTpAccountPtr>() << accountWrapper;
    if (accountWrapper->hasRoster()) {
        builder = syncRosterAccountsContactsBuilder(accounts, true);
    } else {
        builder = syncNoRosterAccountsContactsBuilder(accounts);
    }

    /* If it is not the first time account gets a roster, or if account has
     * no contacts, execute query without notify import progress */
    if (!accountWrapper->isNewAccount() || accountWrapper->contacts().isEmpty()) {
        CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
        connect(query,
                SIGNAL(finished(CDTpSparqlQuery *)),
                SLOT(onSparqlQueryFinished(CDTpSparqlQuery *)));
        return;
    }

    /* Notify import progress */
    Q_EMIT syncStarted(accountWrapper);
    CDTpAccountsSparqlQuery *query = new CDTpAccountsSparqlQuery(accountWrapper, builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSyncOperationEnded(CDTpSparqlQuery *)));
}

void CDTpStorage::syncAccountContacts(CDTpAccountPtr accountWrapper,
        const QList<CDTpContactPtr> &contactsAdded,
        const QList<CDTpContactPtr> &contactsRemoved)
{
    CDTpQueryBuilder builder;

    if (!contactsAdded.isEmpty()) {
        builder.append(createContactsBuilder(contactsAdded));

        // Update nie:contentLastModified on all nco:PersonContact bound to contacts
        Insert i(Insert::Replace);
        Graph g(defaultGraph);
        g.addPattern(imContactVar, nie::contentLastModified::resource(), literalTimeStamp());
        i.addData(g);
        i.addRestriction(imContactVar, imAddressChain, imAddressVar);
        i.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAddressVar), literalIMAddressList(contactsAdded))));
        builder.append(i);
    }

    if (!contactsRemoved.isEmpty()) {
        cancelQueuedUpdates(contactsRemoved);
        builder.append(removeContactsBuilder(accountWrapper, contactsRemoved));
    }

    CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSparqlQueryFinished(CDTpSparqlQuery *)));
}

/* Use this only in offline mode - use syncAccountContacts in online mode */
void CDTpStorage::removeAccountContacts(const QString &accountPath, const QStringList &contactIds)
{
    CDTpSparqlQuery *query = new CDTpSparqlQuery(removeContactsBuilder(accountPath, contactIds), this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onSparqlQueryFinished(CDTpSparqlQuery *)));
}

void CDTpStorage::updateContact(CDTpContactPtr contactWrapper, CDTpContact::Changes changes)
{
    if (!mUpdateQueue.contains(contactWrapper)) {
        mUpdateQueue.insert(contactWrapper, changes);
    } else {
        mUpdateQueue[contactWrapper] |= changes;
    }

    if (!mUpdateRunning) {
        mUpdateTimer.start();
    }
}

void CDTpStorage::onUpdateQueueTimeout()
{
    debug() << "Update" << mUpdateQueue.count() << "contacts";

    CDTpQueryBuilder builder;
    Graph g(privateGraph);

    QList<CDTpContactPtr> allContacts;
    QList<CDTpContactPtr> infoContacts;
    QList<CDTpContactPtr> capsContacts;
    QHash<CDTpContactPtr, CDTpContact::Changes>::const_iterator iter;
    for (iter = mUpdateQueue.constBegin(); iter != mUpdateQueue.constEnd(); iter++) {
        CDTpContactPtr contactWrapper = iter.key();
        CDTpContact::Changes changes = iter.value();
        if (!contactWrapper->isVisible()) {
            continue;
        }

        allContacts << contactWrapper;

        // Special case for Capabilities and Information changes
        if (changes & CDTpContact::Capabilities) {
            capsContacts << contactWrapper;
        }
        if (changes & CDTpContact::Information) {
            infoContacts << contactWrapper;
        }

        // Add IMAddress changes
        addContactChanges(g, literalIMAddress(contactWrapper),
                changes, contactWrapper->contact());
    }
    mUpdateQueue.clear();

    if (allContacts.isEmpty()) {
        debug() << "  None needs update";
        return;
    }

    Insert i(Insert::Replace);
    i.addData(g);
    builder.append(i);

    // prepend delete nco:imCapability for contacts who's caps changed
    if (!capsContacts.isEmpty()) {
        Delete d;
        Variable caps;
        d.addData(imAddressVar, nco::imCapability::resource(), caps);
        d.addRestriction(imAddressVar, nco::imCapability::resource(), caps);
        d.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAddressVar), literalIMAddressList(capsContacts))));
        builder.prepend(d);
    }

    // delete ContactInfo for contacts who's info changed
    if (!infoContacts.isEmpty()) {
        Delete d;
        d.addRestriction(imContactVar, imAddressChain, imAddressVar);
        d.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAddressVar), literalIMAddressList(infoContacts))));
        addRemoveContactInfo(d, imAddressVar, imContactVar);
        builder.append(d);
    }

    // Create ContactInfo for each contact who's info changed
    Q_FOREACH (const CDTpContactPtr contactWrapper, infoContacts) {
        builder.append(createContactInfoBuilder(contactWrapper));
    }

    // Update nie:contentLastModified on all nco:PersonContact bound to contacts
    i = Insert(Insert::Replace);
    g = Graph(defaultGraph);
    g.addPattern(imContactVar, nie::contentLastModified::resource(), literalTimeStamp());
    i.addData(g);
    i.addRestriction(imContactVar, imAddressChain, imAddressVar);
    i.setFilter(Filter(Functions::in.apply(Functions::str.apply(imAddressVar), literalIMAddressList(allContacts))));
    builder.append(i);

    // Launch the query
    mUpdateRunning = true;
    CDTpSparqlQuery *query = new CDTpSparqlQuery(builder, this);
    connect(query,
            SIGNAL(finished(CDTpSparqlQuery *)),
            SLOT(onUpdateFinished(CDTpSparqlQuery *)));
}

void CDTpStorage::onUpdateFinished(CDTpSparqlQuery *query)
{
    onSparqlQueryFinished(query);

    mUpdateRunning = false;
    if (!mUpdateQueue.isEmpty()) {
        mUpdateTimer.start();
    }
}

void CDTpStorage::cancelQueuedUpdates(const QList<CDTpContactPtr> &contacts)
{
    Q_FOREACH (const CDTpContactPtr &contactWrapper, contacts) {
        mUpdateQueue.remove(contactWrapper);
    }
}

void CDTpStorage::onSyncOperationEnded(CDTpSparqlQuery *query)
{
    onSparqlQueryFinished(query);

    CDTpAccountsSparqlQuery *accountsQuery = qobject_cast<CDTpAccountsSparqlQuery*>(query);
    QList<CDTpAccountPtr> accounts = accountsQuery->accounts();

    Q_FOREACH (const CDTpAccountPtr &accountWrapper, accounts) {
        Q_EMIT syncEnded(accountWrapper, accountWrapper->contacts().count(), 0);
    }
}

void CDTpStorage::onSparqlQueryFinished(CDTpSparqlQuery *query)
{
    if (query->hasError()) {
        QSparqlError e = query->error();
        ErrorCode code = ErrorUnknown;
        if (e.type() == QSparqlError::BackendError && e.number() == TRACKER_SPARQL_ERROR_NO_SPACE) {
            code = ErrorNoSpace;
        }
        Q_EMIT error(code, e.message());
    }
}

