# This file is part of Contacts daemon
#
# Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
#
# Contact:  Nokia Corporation (info@qt.nokia.com)
#
# GNU Lesser General Public License Usage
# This file may be used under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation and appearing in the
# file LICENSE.LGPL included in the packaging of this file.  Please review the
# following information to ensure the GNU Lesser General Public License version
# 2.1 requirements will be met:
# http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
#
# In addition, as a special exception, Nokia gives you certain additional rights.
# These rights are described in the Nokia Qt LGPL Exception version 1.1, included
# in the file LGPL_EXCEPTION.txt in this package.
#
# Other Usage
# Alternatively, this file may be used in accordance with the terms and
# conditions contained in a signed written agreement between you and Nokia.

TEMPLATE = lib
QT -= gui
QT += dbus

system(qdbusxml2cpp -c GarbageCollectorAdaptor -a garbagecollectoradaptor.h:garbagecollectoradaptor.cpp com.nokia.contacts.GarbageCollector1.xml)

CONFIG += plugin qtsparql

INCLUDEPATH += $$TOP_SOURCEDIR/src
DEPENDPATH += $$TOP_SOURCEDIR/src

DEFINES += ENABLE_DEBUG

HEADERS  = \
    gcplugin.h \
    garbagecollectoradaptor.h

SOURCES  = \
    gcplugin.cpp \
    garbagecollectoradaptor.cpp

TARGET = garbagecollectorplugin
target.path = $$LIBDIR/contactsd-1.0/plugins

xml.files = com.nokia.contacts.GarbageCollector1.xml
xml.path = $$INCLUDEDIR/contactsd-1.0

INSTALLS += target xml

OTHER_FILES += \
    com.nokia.contacts.GarbageCollector1.xml
