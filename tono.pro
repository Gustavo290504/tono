QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release
TARGET = tono
TEMPLATE = app

HEADERS += \
    src/lyrics.h \
    src/mpris.h \
    src/players.h \
    src/settings.h \
    src/spotify.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/lyrics.cpp \
    src/mpris.cpp \
    src/players.cpp \
    src/settings.cpp \
    src/spotify.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
