#include "dbusplugin.h"
#include <QtPlugin>
#include <QTimer>

DbusPlugin::DbusPlugin()
{
    fakeSignals();
}

DbusPlugin::~DbusPlugin()
{
}

void DbusPlugin::init()
{
}

DbusPlugin::MetaData DbusPlugin::metaData()
{
    MetaData data;
    data[metaDataKeyName] = QVariant(QString("dbusplugin"));
    return data;
}

void DbusPlugin::fakeSignals()
{
   Q_EMIT importStarted(QString("IM Service"),
                QString("/fake/account/"));

   Q_EMIT importEnded(QString("IM Service"),
           QString("/fake/account/"), 10, 30, 1);
}

Q_EXPORT_PLUGIN2(dbusplugin, DbusPlugin)
