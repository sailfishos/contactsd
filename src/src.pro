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

TEMPLATE = app
TARGET = contactsd

VERSIONED_TARGET = $$TARGET-1.0

QT += dbus
QT -= gui

system(qdbusxml2cpp -c ContactsImportProgressAdaptor -a contactsimportprogressadaptor.h:contactsimportprogressadaptor.cpp com.nokia.contacts.importprogress.xml)

include(../lib.pri)

TRANSLATIONS_INSTALL_PATH = "/usr/share/translations"
DEFINES += TRANSLATIONS_INSTALL_PATH=\"\\\"\"$${TRANSLATIONS_INSTALL_PATH}\"\\\"\"

CONFIG += link_pkgconfig
PKGCONFIG += buteosyncfw5

packagesExist(qt5-boostable) {
    DEFINES += HAS_BOOSTER
    PKGCONFIG += qt5-boostable
} else {
    warning("qt5-boostable not available; startup times will be slower")
}

HEADERS += contactsd.h \
    contactsdpluginloader.h \
    importstate.h \
    contactsimportprogressadaptor.h \
    synctrigger.h \
    base-plugin.h

SOURCES += main.cpp \
    contactsd.cpp \
    contactsdpluginloader.cpp \
    importstate.cpp \
    contactsimportprogressadaptor.cpp \
    synctrigger.cpp

DEFINES += VERSION=\\\"$${VERSION}\\\"
DEFINES += CONTACTSD_PLUGINS_DIR=\\\"$$LIBDIR/$${VERSIONED_TARGET}/plugins\\\"

xml.files = com.nokia.contacts.importprogress.xml
xml.path = $$INCLUDEDIR/$${VERSIONED_TARGET}

target.path = $$BINDIR

INSTALLS += target xml
