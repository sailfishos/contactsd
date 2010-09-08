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

unix {
    DEFINES += CONTACTSD_LOG_DIR=\\\"/var/log\\\"
    DEFINES += CONTACTSD_PLUGINS_DIR=\\\"/usr/lib/contactsd-1.0/plugins\\\"
}

headers.files = contactsdplugininterface.h
headers.path = /usr/include/contactsd-1.0

target.path = /usr/bin
INSTALLS += target headers
