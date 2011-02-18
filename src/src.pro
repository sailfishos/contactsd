TEMPLATE = app
TARGET = contactsd

QT += dbus
QT -= gui

system(qdbusxml2cpp -c ContactsImportProgressAdaptor -a contactsimportprogressadaptor.h:contactsimportprogressadaptor.cpp com.nokia.contacts.importprogress.xml)

INCLUDEPATH += $$TOP_SOURCEDIR/lib
LIBS += -export-dynamic
DEFINES += ENABLE_DEBUG

HEADERS += contactsd.h \
    contactsdpluginloader.h \
    importstate.h \
    importstateconst.h \
    contactsimportprogressadaptor.h \
    debug.h \
    base-plugin.h

SOURCES += main.cpp \
    contactsd.cpp \
    contactsdpluginloader.cpp \
    importstate.cpp \
    contactsimportprogressadaptor.cpp \
    debug.cpp \
    base-plugin.cpp

QMAKE_CLEAN += \
    contactsimportprogressadaptor.h \
    contactsimportprogressadaptor.cpp

DEFINES += VERSION=\\\"$${VERSION}\\\"
DEFINES += CONTACTSD_LOG_DIR=\\\"$$LOCALSTATEDIR/log\\\"
DEFINES += CONTACTSD_PLUGINS_DIR=\\\"$$LIBDIR/contactsd-1.0/plugins\\\"

headers.files = BasePlugin base-plugin.h \
    Debug debug.h \
    ImportStateConst importstateconst.h
headers.path = $$INCLUDEDIR/contactsd-1.0/Contactsd

xml.files = com.nokia.contacts.importprogress.xml
xml.path = $$INCLUDEDIR/contactsd-1.0

target.path = $$BINDIR
INSTALLS += target headers xml
