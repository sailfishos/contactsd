TEMPLATE = subdirs
CONFIG += ordered
QT -= gui
SUBDIRS += src/telepathysupport/telepathysupport.pro
SUBDIRS += src/src.pro
SUBDIRS += plugins tests

pkgconfig.path=/usr/lib/pkgconfig
pkgconfig.files=contactsd-1.0.pc

INSTALLS += pkgconfig
