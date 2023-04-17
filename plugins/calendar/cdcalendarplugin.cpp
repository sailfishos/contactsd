/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2020 Jolla Ltd.
 ** Copyright (c) 2020 Open Mobile Platform LLC.
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **/

#include "cdcalendarcontroller.h"
#include "cdcalendarplugin.h"
#include "debug.h"

using namespace Contactsd;

CDCalendarPlugin::CDCalendarPlugin()
    : mController(0)
{
}

CDCalendarPlugin::~CDCalendarPlugin()
{
}

void CDCalendarPlugin::init()
{
    qCDebug(lcContactsd) << "Initializing contactsd Calendar plugin";

    mController = new CDCalendarController(this);
}

CDCalendarPlugin::MetaData CDCalendarPlugin::metaData()
{
    MetaData data;
    data[metaDataKeyName]    = QVariant(QLatin1String("Calendar"));
    data[metaDataKeyVersion] = QVariant(QLatin1String("0.1"));
    data[metaDataKeyComment] = QVariant(QLatin1String("contactsd Calendar plugin"));
    return data;
}
