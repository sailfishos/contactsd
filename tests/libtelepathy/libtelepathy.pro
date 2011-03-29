TARGET = telepathy
TEMPLATE = lib

CONFIG -= warn_on qt
CONFIG += link_pkgconfig
CONFIG += staticlib
PKGCONFIG += telepathy-glib

HEADERS += \
    contacts-conn.h \
    contact-list-manager.h \
    debug.h \
    simple-account.h \
    simple-account-manager.h \
    simple-conn.h \
    textchan-null.h \
    util.h

SOURCES += \
    contacts-conn.c \
    contact-list-manager.c \
    debug.c \
    simple-account.c \
    simple-account-manager.c \
    simple-conn.c \
    textchan-null.c \
    util.c

