TEMPLATE = app
TARGET = contactsd

QT += dbus

HEADERS += contactsd.h \
    contactsdpluginloader.h \
    contactsdplugininterface.h \
    importnotifierdbusadaptor.h \
    logger.h
SOURCES += main.cpp \
    contactsd.cpp \
    contactsdpluginloader.cpp \
    importnotifierdbusadaptor.cpp \
    logger.cpp

BUILDDATE = $$system(date +%F)
BUILDTIME = $$system(date +%T)
REVISION = $$system(cat $$TOP_SOURCEDIR/.git/refs/heads/master)

DEFINES += VERSION_INFO=\\\"$${VERSION}\\\"
DEFINES += BUILDDATE_INFO=\\\"$${BUILDDATE}\\\"
DEFINES += BUILDTIME_INFO=\\\"$${BUILDTIME}\\\"
DEFINES += REVISION_INFO=\\\"$${REVISION}\\\"

unix {
    DEFINES += CONTACTSD_LOG_DIR=\\\"$$LOCALSTATEDIR/log\\\"
    DEFINES += CONTACTSD_PLUGINS_DIR=\\\"$$LIBDIR/contactsd-1.0/plugins\\\"
}

headers.files = contactsdplugininterface.h
headers.path = $$INCLUDEDIR/contactsd-1.0

target.path = $$BINDIR
INSTALLS += target headers
