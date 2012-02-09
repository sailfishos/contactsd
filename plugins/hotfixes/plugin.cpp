/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
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

#include "base-plugin.h"
#include "hotfixes.h"

namespace Contactsd {

class HotFixesPlugin : public BasePlugin
{
    Q_OBJECT

public:
    HotFixesPlugin()
        : m_hotfixes(0)
    {
    }

    void init()
    {
        m_hotfixes = new HotFixes(sparqlConnection(), this);
    }

    QMap<QString, QVariant> metaData()
    {
        MetaData data;
        data[metaDataKeyName]    = QVariant(QString::fromLatin1("contacts-hotfixes"));
        data[metaDataKeyVersion] = QVariant(QString::fromLatin1("1"));
        data[metaDataKeyComment] = QVariant(QString::fromLatin1("Hotfixes for the contacts framework"));
        return data;
    }

private:
    HotFixes *m_hotfixes;
};

} // namespace Contactsd

Q_EXPORT_PLUGIN2(garbagecollectorplugin, Contactsd::HotFixesPlugin)

#include "plugin.moc"
