#-------------------------------------------------
#
# Project created by QtCreator 2012-02-06T12:57:23
#
#-------------------------------------------------

TEMPLATE = lib
TARGET = contactshotfixesplugin

CONFIG += plugin qtsparql qmsystem2
QT += core
QT -= gui

INCLUDEPATH += $$TOP_SOURCEDIR/src
DEPENDPATH += $$TOP_SOURCEDIR/src
DEFINES += ENABLE_DEBUG QT_ASCII_CAST_WARNINGS

target.path = $$LIBDIR/contactsd-1.0/plugins

SOURCES += \
    hotfixes.cpp \
    plugin.cpp

HEADERS += \
    hotfixes.h

INSTALLS += target
