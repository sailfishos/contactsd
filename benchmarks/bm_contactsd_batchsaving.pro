CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4

INCLUDEPATH += $$PWD/../src/telepathysupport
INCLUDEPATH = $$PWD/../plugins/telepathy

LIBS += -L../src/telepathysupport
LIBS += -lcontactstelepathysupport

LIBS += -L../plugins/telepathy
LIBS += -L/usr/lib/contactsd-1.0/plugins
LIBS += -ltelepathycollectorplugin

include($$PWD/../tests/common/common.pri)

SOURCES += bm_contactsd_batchsaving.cpp


INSTALLS += target
target.path = $$PREFIX/bin
