# include version number for the plugin
TEMPLATE = lib
QT += dbus xml
CONFIG	+= plugin

QMAKE_LIBDIR += ../../src/telepathysupport


CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 \
             accounts-qt \
	     qttracker

LIBS += -lcontactstelepathysupport

INCLUDEPATH += $$PWD/../../src/telepathysupport/
INCLUDEPATH += $$PWD/../../src

#DESTDIR	 = 
HEADERS  = telepathyplugin.h \
           trackersink.h \
           accountservicemapper.h
	   
SOURCES  = telepathyplugin.cpp \
           trackersink.cpp \
           accountservicemapper.cpp

TARGET = telepathycollectorplugin
target.path = /usr/lib/contactsd-1.0/plugins
INSTALLS += target
