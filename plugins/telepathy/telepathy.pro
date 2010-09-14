TEMPLATE = lib
QT += dbus

CONFIG += plugin

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker

INCLUDEPATH += $$TOP_SOURCEDIR/src

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
target.path = $$LIBDIR/contactsd-1.0/plugins
INSTALLS += target
