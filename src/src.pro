TEMPLATE = app
TARGET = contactsd

QT += dbus

system(qdbusxml2cpp -c ContactsImportProgressAdaptor -a contactsimportprogressadaptor.h:contactsimportprogressadaptor.cpp com.nokia.contacts.importprogress.xml)

HEADERS += contactsd.h \
    contactsdpluginloader.h \
    contactsdplugininterface.h \
    importstate.h \
    logger.h \
    contactsimportprogressadaptor.h \
    importstateconst.h

SOURCES += main.cpp \
    contactsd.cpp \
    contactsdpluginloader.cpp \
    importstate.cpp \
    logger.cpp \
    contactsimportprogressadaptor.cpp

QMAKE_CLEAN += \
    contactsimportprogressadaptor.h \
    contactsimportprogressadaptor.cpp

DEFINES += VERSION=\\\"$${VERSION}\\\"

DEFINES += CONTACTSD_LOG_DIR=\\\"$$LOCALSTATEDIR/log\\\"
DEFINES += CONTACTSD_PLUGINS_DIR=\\\"$$LIBDIR/contactsd-1.0/plugins\\\"

headers.files = ContactsdPluginInterface \
                contactsdplugininterface.h \
                importstateconst.h
headers.path = $$INCLUDEDIR/contactsd-1.0

xml.files = com.nokia.contacts.importprogress.xml
xml.path = $$INCLUDEDIR/contactsd-1.0

target.path = $$BINDIR
INSTALLS += target headers xml
