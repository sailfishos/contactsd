TARGET = test-contactsd

QT -= gui
test.depends = all
QMAKE_EXTRA_TARGETS += test

CONFIG += test
QT += testlib
QT += dbus

SOURCE_DIR = $$PWD/../../src

INCLUDEPATH += $$SOURCE_DIR

HEADERS += test-contactsd.h \
           $$SOURCE_DIR/contactsd.h \
	   $$SOURCE_DIR/importnotifierdbusadaptor.h \
           $$SOURCE_DIR/contactsdpluginloader.h

SOURCES += test-contactsd.cpp \
           $$SOURCE_DIR/contactsd.cpp \
	   $$SOURCE_DIR/importnotifierdbusadaptor.cpp  \
           $$SOURCE_DIR/contactsdpluginloader.cpp

DEFINES += CONTACTSD_PLUGINS_DIR=\\\"/usr/lib/contactsd-1.0/plugins\\\"
