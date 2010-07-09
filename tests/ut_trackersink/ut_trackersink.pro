test.depends = all
QMAKE_EXTRA_TARGETS += test
QCONTACTS_TRACKER_BACKENDDIR = $$PWD/../../src/telepathysupport

CONFIG += test mobility

MOBILITY += contacts
QT += dbus network

CONFIG += link_pkgconfig
PKGCONFIG += TelepathyQt4 qttracker

OBJECTS_DIR = .obj

INCLUDEPATH += $$QCONTACTS_TRACKER_BACKENDDIR 

## Include source files under test.
HEADERS += $$QCONTACTS_TRACKER_BACKENDDIR/trackersink.h \
           $$QCONTACTS_TRACKER_BACKENDDIR/telepathycontact.h \
           $$QCONTACTS_TRACKER_BACKENDDIR/telepathycontroller.h  \
           $$QCONTACTS_TRACKER_BACKENDDIR/contactcache.h \
           $$QCONTACTS_TRACKER_BACKENDDIR/abstractcontactsink.h \
           $$QCONTACTS_TRACKER_BACKENDDIR/pendingrosters.h

SOURCES += $$QCONTACTS_TRACKER_BACKENDDIR/trackersink.cpp \
           $$QCONTACTS_TRACKER_BACKENDDIR/telepathycontact.cpp \
           $$QCONTACTS_TRACKER_BACKENDDIR/telepathycontroller.cpp  \
           $$QCONTACTS_TRACKER_BACKENDDIR/pendingrosters.cpp


## Include unit test files
HEADERS += ut_trackersink.h

SOURCES += ut_trackersink.cpp
