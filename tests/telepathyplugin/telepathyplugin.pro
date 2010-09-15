include($$TOP_SOURCEDIR/check.pri)

test.depends = all
QMAKE_EXTRA_TARGETS += test

CONFIG += test mobility qt

MOBILITY += contacts
QT += testlib

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker telepathy-glib
LIBS += -Wl,-rpath,$$TOP_BUILDDIR/tests/lib/glib -L$$TOP_BUILDDIR/tests/lib/glib -ltestsglib
INCLUDEPATH += $$TOP_SOURCEDIR

OBJECTS_DIR = .obj

HEADERS += test-telepathy-plugin.h test.h
SOURCES += test-telepathy-plugin.cpp test.cpp
