include($$TOP_SOURCEDIR/check.pri)

test.depends = all
QMAKE_EXTRA_TARGETS += test

CONFIG += test mobility

MOBILITY += contacts
QT += testlib

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker

OBJECTS_DIR = .obj

HEADERS += test-telepathy-plugin.h
SOURCES += test-telepathy-plugin.cpp
