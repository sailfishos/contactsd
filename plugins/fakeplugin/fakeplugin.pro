TEMPLATE = lib
QT += dbus

CONFIG += plugin

CONFIG(coverage):{
QMAKE_CXXFLAGS += -c -g  --coverage -ftest-coverage -fprofile-arcs
LIBS += -lgcov
}

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker

INCLUDEPATH += $$TOP_SOURCEDIR/src

HEADERS  = fakeplugin.h

SOURCES  = fakeplugin.cpp

TARGET = fakeplugin
target.path = $$LIBDIR/contactsd-1.0/plugins
INSTALLS += target
