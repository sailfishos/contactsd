# include version number for the plugin

TEMPLATE = lib
QT += dbus
CONFIG += plugin
INCLUDEPATH += ../../src

HEADERS  = helloworld.h

SOURCES  = helloworld.cpp

TARGET = helloworld
target.path = /usr/lib/contactsd-1.0/plugins/
INSTALLS += target
