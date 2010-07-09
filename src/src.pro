BUILDDATE = $$system(date +%F)
BUILDTIME = $$system(date +%T)
REVISION = $$system(cat $$PWD/../.git/refs/heads/master)

DEFINES += VERSION_INFO=\\\"$${VERSION}\\\"
DEFINES += BUILDDATE_INFO=\\\"$${BUILDDATE}\\\"
DEFINES += BUILDTIME_INFO=\\\"$${BUILDTIME}\\\"
DEFINES += REVISION_INFO=\\\"$${REVISION}\\\"

TEMPLATE = app
TARGET = contactsd
MOC_DIR = ./.moc

CONFIG += mobility
MOBILITY += contacts

DEPENDPATH += .
QT += dbus \
    core

TESTS_DIR = $$PWD/../tests 

HEADERS += contactsdaemon.h \
    contactsdpluginloader.h \
    contactsdplugininterface.h \
    logger.h
SOURCES += main.cpp \
    contactsdaemon.cpp \
    contactsdpluginloader.cpp \
    logger.cpp

unix {
    DEFINES += CONTACTSD_PLUGIN_PATH=\\\"/usr/lib/contactsd-1.0/plugins\\\"
}

headers.files = contactsdplugininterface.h
headers.path = /usr/include/contactsd-1.0

target.path = /usr/bin
INSTALLS += target headers
