#ifndef CONTACTSDPLUGININTERFACE_H_
#define CONTACTSDPLUGININTERFACE_H_

#include <QObject>
#include <QMap>
#include <QString>

const QString CONTACTSD_PLUGIN_VERSION = "version";
const QString CONTACTSD_PLUGIN_NAME    = "name";
const QString CONTACTSD_PLUGIN_COMMENT = "comment";

class ContactsdPluginInterface
{
public:
    typedef QMap<QString, QVariant> PluginMetaData;

    virtual ~ContactsdPluginInterface () { };
    virtual void init() = 0;
    virtual PluginMetaData metaData() = 0;
};

Q_DECLARE_INTERFACE(ContactsdPluginInterface, "com.nokia.contactsd.Plugin/1.0")
#endif
