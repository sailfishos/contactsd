TEMPLATE = app
TARGET = test-account-manager

CONFIG += link_pkgconfig
PKGCONFIG += telepathy-glib
LIBS += -Wl,-rpath,$$TOP_BUILDDIR/tests/lib/glib -L$$TOP_BUILDDIR/tests/lib/glib -ltestsglib
INCLUDEPATH += $$TOP_SOURCEDIR

SOURCES = test-account-manager.c
