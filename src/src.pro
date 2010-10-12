TEMPLATE = app
TARGET = contactsd

QT += dbus

HEADERS += contactsd.h \
    contactsdpluginloader.h \
    contactsdplugininterface.h \
    importnotifierdbusadaptor.h \
    importstate.h \
    logger.h
SOURCES += main.cpp \
    contactsd.cpp \
    contactsdpluginloader.cpp \
    importnotifierdbusadaptor.cpp \
    importstate.cpp \
    logger.cpp

DEFINES += VERSION=\\\"$${VERSION}\\\"

DEFINES += CONTACTSD_LOG_DIR=\\\"$$LOCALSTATEDIR/log\\\"
DEFINES += CONTACTSD_PLUGINS_DIR=\\\"$$LIBDIR/contactsd-1.0/plugins\\\"

headers.files = ContactsdPluginInterface contactsdplugininterface.h
headers.path = $$INCLUDEDIR/contactsd-1.0

target.path = $$BINDIR
INSTALLS += target headers
