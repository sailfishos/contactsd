CONFIG += test

QT -= gui
QT += testlib
QT += dbus

CONFIG(coverage):{
QMAKE_CXXFLAGS += -c --coverage -ftest-coverage -fprofile-arcs
LIBS += -lgcov
}

TARGET = ut_contactsd
target.path = /usr/bin/

INCLUDEPATH += $$TOP_SOURCEDIR/src

HEADERS += test-contactsd.h \
    $$TOP_SOURCEDIR/src/contactsd.h \
    $$TOP_SOURCEDIR/src/contactsimportprogressadaptor.h \
    $$TOP_SOURCEDIR/src/contactsdpluginloader.h \
    $$TOP_SOURCEDIR/src/importstate.h

SOURCES += test-contactsd.cpp \
    $$TOP_SOURCEDIR/src/contactsd.cpp \
    $$TOP_SOURCEDIR/src/contactsimportprogressadaptor.cpp  \
    $$TOP_SOURCEDIR/src/contactsdpluginloader.cpp \
    $$TOP_SOURCEDIR/src/importstate.cpp

DEFINES += CONTACTSD_PLUGINS_DIR=\\\"$$LIBDIR/contactsd-1.0/plugins\\\"

#gcov stuff
CONFIG(coverage):{
QMAKE_CLEAN += *.gcov $(OBJECTS_DIR)*.gcda $(OBJECTS_DIR)*.gcno gcov.analysis gcov.analysis.summary
gcov.target = gcov
cov.CONFIG = recursive
gcov.commands = for d in $$SOURCES; do (gcov -a -c -o $(OBJECTS_DIR) \$$$$d >> gcov.analysis ); done;
gcov.depends = $(TARGET)
QMAKE_EXTRA_TARGETS += gcov
}

INSTALLS += target
