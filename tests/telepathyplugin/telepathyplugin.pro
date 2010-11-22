include($$TOP_SOURCEDIR/check.pri)

CONFIG += test qt mobility

QT += testlib
MOBILITY += contacts

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker telepathy-glib

DEFINES += QT_NO_KEYWORDS

LIBS += -Wl,-rpath,$$TOP_BUILDDIR/tests/lib/glib -L$$TOP_BUILDDIR/tests/lib/glib -ltestsglib
INCLUDEPATH += $$TOP_SOURCEDIR

CONFIG(coverage): {
QMAKE_CXXFLAGS += -c -g --coverage -ftest-coverage -fprofile-arcs
LIBS += -lgcov
}

TARGET = test-telepathyplugin

HEADERS += test-telepathy-plugin.h \
    test.h

SOURCES += test-telepathy-plugin.cpp \
    test.cpp

#for gcov stuff
CONFIG(coverage): {
INCLUDEPATH += $$TOP_SOURCEDIR/src
HEADERS += $$TOP_SOURCEDIR/plugins/telepathy/cdtpaccount.h \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpcontact.h \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpcontroller.h \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpplugin.h \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpstorage.h


SOURCES += $$TOP_SOURCEDIR/plugins/telepathy/cdtpaccount.cpp \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpcontact.cpp \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpcontroller.cpp \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpplugin.cpp \
    $$TOP_SOURCEDIR/plugins/telepathy/cdtpstorage.cpp

#to use start the daemon from code

HEADERS += $$TOP_SOURCEDIR/src/contactsd.h \
    $$TOP_SOURCEDIR/src/contactsimportprogressadaptor.h \
    $$TOP_SOURCEDIR/src/contactsdpluginloader.h \
    $$TOP_SOURCEDIR/src/importstate.h

SOURCES += $$TOP_SOURCEDIR/src/contactsd.cpp \
    $$TOP_SOURCEDIR/src/contactsimportprogressadaptor.cpp  \
    $$TOP_SOURCEDIR/src/contactsdpluginloader.cpp \
    $$TOP_SOURCEDIR/src/importstate.cpp

DEFINES += CONTACTSD_PLUGINS_DIR=\\\"$$TOP_SOURCEDIR/plugins/telepathy/\\\"

QMAKE_CLEAN += *.gcov $(OBJECTS_DIR)*.gcda $(OBJECTS_DIR)*.gcno gcov.analysis gcov.analysis.summary

gcov.target = gcov
gcov.CONFIG = recursive
gcov.commands += for d in $$SOURCES; do (gcov -a -c -o $(OBJECTS_DIR) \$$$$d >> gcov.analysis ); done;
gcov.depends = $(TARGET)

QMAKE_EXTRA_TARGETS += gcov
}

