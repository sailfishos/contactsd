TEMPLATE = subdirs
CONFIG += ordered
QT -= gui

SUBDIRS += src plugins tests

pkgconfig.path=$$LIBDIR/pkgconfig
pkgconfig.files=contactsd-1.0.pc
INSTALLS += pkgconfig

check.target = check
check.CONFIG = recursive
QMAKE_EXTRA_TARGETS += check

confclean.depends += distclean
confclean.commands += \
    $(DEL_FILE) $$TOP_BUILDDIR/.qmake.cache \
    $(DEL_FILE) $$TOP_BUILDDIR/contactsd-1.0.pc \
    $(DEL_FILE) $$TOP_BUILDDIR/tests/dbus-1/session.conf \
    $(DEL_FILE) $$TOP_BUILDDIR/tests/dbus-1/services/org.freedesktop.Telepathy.AccountManager.service
QMAKE_EXTRA_TARGETS += confclean
