TARGET = ut_contactsd

QT -= gui
test.depends = all
QMAKE_EXTRA_TARGETS += test

CONFIG += test
QT += testlib
QT += dbus

SOURCE_DIR = $$PWD/../../src

INCLUDEPATH += $$SOURCE_DIR

HEADERS += ut_contactsd.h \
           $$SOURCE_DIR/contactsdaemon.h \
	   $$SOURCE_DIR/importnotifierdbusadaptor.h \
           $$SOURCE_DIR/contactsdpluginloader.h

SOURCES += ut_contactsd.cpp \
           $$SOURCE_DIR/contactsdaemon.cpp \
	   $$SOURCE_DIR/importnotifierdbusadaptor.cpp  \
           $$SOURCE_DIR/contactsdpluginloader.cpp

DEFINES += CONTACTSD_PLUGIN_PATH=\\\"/usr/lib/contactsd-1.0/plugins\\\"
