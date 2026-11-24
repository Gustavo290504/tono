QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release
TARGET = tono
TEMPLATE = app

HEADERS += \
    src/mpris.h \
    src/players.h \
    src/settings.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/mpris.cpp \
    src/players.cpp \
    src/settings.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
