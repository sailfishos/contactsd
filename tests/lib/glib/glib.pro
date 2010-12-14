TEMPLATE = lib
CONFIG += staticlib

CONFIG -= warn_on

CONFIG -= qt

CONFIG += link_pkgconfig
PKGCONFIG += telepathy-glib

TARGET = testsglib

SOURCES += \
    contacts-conn.c \
    contacts-conn.h \
    contact-list-manager.c \
    contact-list-manager.h \
    debug.h \
    simple-account.c \
    simple-account.h \
    simple-account-manager.c \
    simple-account-manager.h \
    simple-conn.c \
    simple-conn.h \
    textchan-null.c \
    textchan-null.h \
    util.c \
    util.h

