CONFIG += qt mobility

MOBILITY += contacts

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker telepathy-glib

DEFINES += QT_NO_KEYWORDS

LIBS += -Wl,-rpath,$$TOP_BUILDDIR/tests/lib/glib -L$$TOP_BUILDDIR/tests/lib/glib -ltestsglib
INCLUDEPATH += $$TOP_SOURCEDIR

TARGET = benchmark
HEADERS += benchmark.h
SOURCES += benchmark.cpp

