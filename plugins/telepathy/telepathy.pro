TEMPLATE = lib
QT += dbus xml

CONFIG += plugin

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 \
             accounts-qt \
             qttracker

INCLUDEPATH += $$PWD/../../src

HEADERS  = cdtpaccount.h \
           cdtpcontact.h \
           cdtpcontroller.h \
           cdtpplugin.h \
           cdtpstorage.h

SOURCES  = cdtpaccount.cpp \
           cdtpcontact.cpp \
           cdtpcontroller.cpp \
           cdtpplugin.cpp \
           cdtpstorage.cpp

TARGET = telepathyplugin
target.path = /usr/lib/contactsd-1.0/plugins
INSTALLS += target
