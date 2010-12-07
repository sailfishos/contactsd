#include "fakeplugin.h"
#include <QtPlugin>

FakePlugin::FakePlugin() : mHasActiveImports(0)
{
}

FakePlugin::~FakePlugin()
{
}

void FakePlugin::init()
{
}

bool FakePlugin::hasActiveImports() const
{
    return mHasActiveImports;
}

QMap<QString, QVariant> FakePlugin::metaData()
{
    QMap<QString, QVariant> data;
    return data;
}

Q_EXPORT_PLUGIN2(fakeplugin, FakePlugin)
