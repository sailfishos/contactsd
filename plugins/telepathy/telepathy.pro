TEMPLATE = lib
QT += dbus

CONFIG += plugin qtsparql

CONFIG(coverage):{
QMAKE_CXXFLAGS += -c -g  --coverage -ftest-coverage -fprofile-arcs
LIBS += -lgcov
}

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker

INCLUDEPATH += $$TOP_SOURCEDIR/src

HEADERS  = cdtpaccount.h \
    types.h \
    cdtpcontact.h \
    cdtpcontroller.h \
    cdtpplugin.h \
    cdtpquery.h \
    cdtpstorage.h \
    sparqlconnectionmanager.h

SOURCES  = cdtpaccount.cpp \
    cdtpcontact.cpp \
    cdtpcontroller.cpp \
    cdtpplugin.cpp \
    cdtpquery.cpp \
    cdtpstorage.cpp \
    sparqlconnectionmanager.cpp

TARGET = telepathyplugin
target.path = $$LIBDIR/contactsd-1.0/plugins
INSTALLS += target
