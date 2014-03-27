/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2014 Jolla Ltd.
 **
 ** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **/

#include "cdexportercontroller.h"
#include "cdexporterplugin.h"
#include "debug.h"

using namespace Contactsd;

CDExporterPlugin::CDExporterPlugin()
    : mController(0)
{
}

CDExporterPlugin::~CDExporterPlugin()
{
}

void CDExporterPlugin::init()
{
    debug() << "Initializing contactsd exporter plugin";

    mController = new CDExporterController(this);
}

CDExporterPlugin::MetaData CDExporterPlugin::metaData()
{
    MetaData data;
    data[metaDataKeyName]    = QVariant(QLatin1String("exporter"));
    data[metaDataKeyVersion] = QVariant(QLatin1String("0.1"));
    data[metaDataKeyComment] = QVariant(QLatin1String("contactsd exporter plugin"));
    return data;
}

