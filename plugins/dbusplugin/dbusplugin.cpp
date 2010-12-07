#include "dbusplugin.h"
#include <QtPlugin>
#include <QTimer>

DbusPlugin::DbusPlugin() : mHasActiveImports(0)
{
    fakeSignals();
}

DbusPlugin::~DbusPlugin()
{
}

void DbusPlugin::init()
{
}

bool DbusPlugin::hasActiveImports() const
{
    return mHasActiveImports;
}

QMap<QString, QVariant> DbusPlugin::metaData()
{
    QMap<QString, QVariant> data;
    data["name"] = QVariant(QString("dbusplugin"));
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
