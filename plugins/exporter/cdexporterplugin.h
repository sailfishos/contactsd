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

#ifndef CDEXPORTERPLUGIN_H
#define CDEXPORTERPLUGIN_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

#include "base-plugin.h"

class CDExporterController;

class CDExporterPlugin : public Contactsd::BasePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.nemomobile.contactsd.exporter")

public:
    CDExporterPlugin();
    ~CDExporterPlugin();

    void init();
    MetaData metaData();

private:
    CDExporterController *mController;
};

#endif // CDEXPORTERPLUGIN_H
