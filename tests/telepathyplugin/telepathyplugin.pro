include($$TOP_SOURCEDIR/check.pri)

CONFIG += test qt mobility

QT += testlib
MOBILITY += contacts

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker telepathy-glib

LIBS += -Wl,-rpath,$$TOP_BUILDDIR/tests/lib/glib -L$$TOP_BUILDDIR/tests/lib/glib -ltestsglib
INCLUDEPATH += $$TOP_SOURCEDIR

TARGET = test-telepathyplugin

HEADERS += test-telepathy-plugin.h \
    test.h

SOURCES += test-telepathy-plugin.cpp \
    test.cpp
