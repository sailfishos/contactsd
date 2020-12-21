TEMPLATE = lib
QT -= gui
QT += contacts-private

CONFIG += plugin

CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt5 KF5CalendarCore libmkcal-qt5

DEFINES -= QT_NO_CAST_TO_ASCII
DEFINES -= QT_NO_CAST_FROM_ASCII

INCLUDEPATH += $$TOP_SOURCEDIR/src
DEFINES += ENABLE_DEBUG

HEADERS  = \
    cdcalendarcontroller.h \
    cdcalendarplugin.h

SOURCES  = \
    cdcalendarcontroller.cpp \
    cdcalendarplugin.cpp

TARGET = calendarplugin
target.path = $$LIBDIR/contactsd-1.0/plugins

INSTALLS += target
