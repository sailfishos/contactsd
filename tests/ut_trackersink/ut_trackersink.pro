test.depends = all
QMAKE_EXTRA_TARGETS += test
TELEPATHY_SUPPORT_DIR = $$PWD/../../src/telepathysupport
TELEPATHY_PLUGIN_DIR = $$PWD/../../plugins/telepathy

CONFIG += test mobility

MOBILITY += contacts
QT += testlib

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker

OBJECTS_DIR = .obj

INCLUDEPATH += $$TELEPATHY_SUPPORT_DIR
INCLUDEPATH = $$TELEPATHY_PLUGIN_DIR

# linking instead of compiling in - bug 186193
# force linking against local version
LIBS +=  ../../src/telepathysupport/libcontactstelepathysupport.so

LIBS += -L../../plugins/telepathy
LIBS += -ltelepathycollectorplugin

include($$PWD/../common/common.pri)

## Include source files under test.
# Files are not compiled in until  Bug 186193 -  
# libqttracker asserts when application directly link to it and uses if from libqtcontacts-tracker plugin 
#HEADERS += $$TELEPATHY_PLUGIN_DIR/trackersink.h \
#           $$TELEPATHY_SUPPORT_DIR/telepathycontroller.h  \
#           $$TELEPATHY_SUPPORT_DIR/contactphotocopy.h  \
#           $$TELEPATHY_SUPPORT_DIR/tpcontact.h  \
#           $$TELEPATHY_SUPPORT_DIR/pendingrosters.h

#SOURCES += $$TELEPATHY_PLUGIN_DIR/trackersink.cpp \
#           $$TELEPATHY_SUPPORT_DIR/telepathycontroller.cpp  \
#           $$TELEPATHY_SUPPORT_DIR/contactphotocopy.cpp  \
#           $$TELEPATHY_SUPPORT_DIR/tpcontact.cpp  \
#           $$TELEPATHY_SUPPORT_DIR/pendingrosters.cpp

## Include unit test files
HEADERS += ut_trackersink.h

SOURCES += ut_trackersink.cpp
