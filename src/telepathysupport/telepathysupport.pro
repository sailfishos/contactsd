TARGET = contactstelepathysupport
TEMPLATE = lib

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4

QT += dbus

HEADERS += $$PWD/telepathycontroller.h  \
        $$PWD/contactphotocopy.h \
	   $$PWD/tpcontact.h \
	   $$PWD/telepathyaccount.h \
           $$PWD/pendingrosters.h

SOURCES += $$PWD/telepathycontroller.cpp \
       $$PWD/contactphotocopy.cpp \
	   $$PWD/tpcontact.cpp \
	   $$PWD/telepathyaccount.cpp \
           $$PWD/pendingrosters.cpp

target.path = /usr/lib/
INSTALLS += target
