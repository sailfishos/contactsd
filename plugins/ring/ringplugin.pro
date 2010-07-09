# include version number for the plugin

TEMPLATE = lib
QT += dbus 
CONFIG += plugin

QMAKE_LIBDIR += ../../src/telepathysupport


CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4

LIBS += -lcontactstelepathysupport

INCLUDEPATH += $$PWD/../../src/telepathysupport/
INCLUDEPATH += $$PWD/../../src

HEADERS  = ringplugin.h

SOURCES  = ringplugin.cpp

TARGET = ringplugin
target.path = /usr/lib/contactsd-1.0/plugins
INSTALLS += target
