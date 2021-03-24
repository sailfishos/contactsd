TEMPLATE = lib

QT -= gui
QT += dbus
TARGET = contactsd
TARGET = $$qtLibraryTarget($$TARGET)
TARGETPATH = $$[QT_INSTALL_LIBS]
target.path = $$TARGETPATH

INSTALLS += target

VERSIONED_TARGET = $$TARGET-1.0

HEADERS += \
    debug.h \
    base-plugin.h

SOURCES += \
    debug.cpp \
    base-plugin.cpp

headers.files = \
    BasePlugin base-plugin.h \
    Debug debug.h

headers.path = $$INCLUDEDIR/$${VERSIONED_TARGET}/Contactsd
