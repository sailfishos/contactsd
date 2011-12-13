TEMPLATE = subdirs
CONFIG += ordered
QT -= gui

SUBDIRS += src plugins tests

pkgconfig.path=$$LIBDIR/pkgconfig
pkgconfig.files=contactsd-1.0.pc

backupconf.path=$$PREFIX/share/backup-framework/applications/
backupconf.files=contactsd.conf

INSTALLS += pkgconfig backupconf

check.target = check
check.CONFIG = recursive
QMAKE_EXTRA_TARGETS += check

confclean.depends += distclean
confclean.commands += \
    $(DEL_FILE) $$TOP_BUILDDIR/.qmake.cache \
    $(DEL_FILE) $$TOP_BUILDDIR/contactsd-1.0.pc
QMAKE_EXTRA_TARGETS += confclean

OTHER_FILES += configure

# Run configure script when building the project from tools like QtCreator
isEmpty(CONFIGURED):system('$$PWD/configure')
