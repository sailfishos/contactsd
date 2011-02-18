#include "fakeplugin.h"
#include <QtPlugin>

FakePlugin::FakePlugin()
{
}

FakePlugin::~FakePlugin()
{
}

void FakePlugin::init()
{
}

FakePlugin::MetaData FakePlugin::metaData()
{
    MetaData data;
    return data;
}

Q_EXPORT_PLUGIN2(fakeplugin, FakePlugin)
