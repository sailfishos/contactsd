TEMPLATE = subdirs
CONFIG += ordered
QT -= gui

SUBDIRS += src plugins tests

pkgconfig.path=/usr/lib/pkgconfig
pkgconfig.files=contactsd-1.0.pc

INSTALLS += pkgconfig

check.target = check
check.CONFIG = recursive
QMAKE_EXTRA_TARGETS += check
