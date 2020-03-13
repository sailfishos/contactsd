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

#ifndef CDCALENDARPLUGIN_H
#define CDCALENDARPLUGIN_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

#include "base-plugin.h"

class CDCalendarController;

class CDCalendarPlugin : public Contactsd::BasePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.nemomobile.contactsd.Calendar")

public:
    CDCalendarPlugin();
    ~CDCalendarPlugin();

    void init();
    MetaData metaData();

private:
    CDCalendarController *mController;
};

#endif // CDCALENDARPLUGIN_H
