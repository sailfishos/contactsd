TEMPLATE = lib
QT += dbus

CONFIG += plugin qtsparql
system(qdbusxml2cpp -c BuddyManagementAdaptor -a buddymanagementadaptor.h:buddymanagementadaptor.cpp com.nokia.contacts.buddymanagement.xml)

CONFIG(coverage):{
QMAKE_CXXFLAGS += -c -g  --coverage -ftest-coverage -fprofile-arcs
LIBS += -lgcov
}

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4

INCLUDEPATH += $$TOP_SOURCEDIR/src

HEADERS  = cdtpaccount.h \
    types.h \
    cdtpcontact.h \
    cdtpcontroller.h \
    cdtpplugin.h \
    cdtpquery.h \
    cdtpstorage.h \
    sparqlconnectionmanager.h \
    buddymanagementadaptor.h \
    redliststorage.h

SOURCES  = cdtpaccount.cpp \
    cdtpcontact.cpp \
    cdtpcontroller.cpp \
    cdtpplugin.cpp \
    cdtpquery.cpp \
    cdtpstorage.cpp \
    sparqlconnectionmanager.cpp \
    buddymanagementadaptor.cpp \
    redliststorage.cpp

TARGET = telepathyplugin
target.path = $$LIBDIR/contactsd-1.0/plugins

xml.files = com.nokia.contacts.buddymanagement.xml
xml.path = $$INCLUDEDIR/contactsd-1.0

INSTALLS += target xml

OTHER_FILES += \
    com.nokia.contacts.buddymanagement.xml
