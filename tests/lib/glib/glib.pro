TEMPLATE = lib
CONFIG += staticlib

CONFIG -= warn_on

CONFIG -= qt

CONFIG += link_pkgconfig
PKGCONFIG += telepathy-glib

TARGET = testsglib

SOURCES += \
    contact-list-conn.c \
    contact-list-conn.h \
    contact-list-manager.c \
    contact-list-manager.h \
    contact-list.c \
    contact-list.h
