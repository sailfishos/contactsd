# include version number for the plugin
TEMPLATE = lib
QT += dbus xml
CONFIG	+= plugin

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 \
             accounts-qt \
	     qttracker

INCLUDEPATH += $$PWD/../../src

#DESTDIR	 = 
HEADERS  = telepathyplugin.h \
           trackersink.h \
           accountservicemapper.h \
           contactphotocopy.h \
           pendingrosters.h \
           telepathyaccount.h \
           telepathycontroller.h \
           telepathyplugin.h \
           tpcontact.h

	   
SOURCES  = telepathyplugin.cpp \
           trackersink.cpp \
           accountservicemapper.cpp \
           contactphotocopy.cpp \
           pendingrosters.cpp \
           telepathyaccount.cpp \
           telepathycontroller.cpp \
           tpcontact.cpp \


TARGET = telepathycollectorplugin
target.path = /usr/lib/contactsd-1.0/plugins
INSTALLS += target
