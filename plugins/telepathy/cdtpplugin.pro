TEMPLATE = lib
QT += dbus xml
CONFIG += plugin

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 \
             accounts-qt \
             qttracker

INCLUDEPATH += $$PWD/../../src

HEADERS  = cdtpaccount.h \
           cdtpaccountservicemapper.h \
           cdtpcontroller.h \
           cdtpcontact.h \
           cdtpcontactphotocopy.h \
           cdtppendingrosters.h \
           cdtpplugin.h \
           cdtptrackersink.h

SOURCES  = cdtpaccount.cpp \
           cdtpaccountservicemapper.cpp \
           cdtpcontroller.cpp \
           cdtpcontact.cpp \
           cdtpcontactphotocopy.cpp \
           cdtppendingrosters.cpp \
           cdtpplugin.cpp \
           cdtptrackersink.cpp

TARGET = telepathycollectorplugin
target.path = /usr/lib/contactsd-1.0/plugins
INSTALLS += target
