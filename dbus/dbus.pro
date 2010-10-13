TEMPLATE = lib
TARGET = contactsimportprogressinterface

QT += dbus

system(qdbusxml2cpp -c ContactsImportProgressInterface -p contactsimportprogressinterface.h:contactsimportprogressinterface.cpp com.nokia.contacts.importprogress.xml)

HEADERS += \
    contactsimportprogressinterface.h

SOURCES += \
    contactsimportprogressinterface.cpp

QMAKE_CLEAN += \
    $$HEADERS \
    $$SOURCES \

target.path = $$LIBDIR/contactsd-1.0
header.path = $$INCLUDEDIR/contactsd-1.0
header.files = $$HEADERS

INSTALLS += header target
