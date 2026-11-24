QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = tono_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/mpris.h ../src/players.h ../src/settings.h ../src/theme.h ../src/translator.h
SOURCES += tono_tests.cpp ../src/mpris.cpp ../src/players.cpp ../src/settings.cpp ../src/theme.cpp ../src/translator.cpp
