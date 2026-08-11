QT -= gui
QT += core

CONFIG += console c++11 release
CONFIG -= app_bundle
TEMPLATE = app
TARGET = parser_tests
DESTDIR = $$PWD/../dist

SOURCES += \
    parser_tests.cpp \
    ../src/frameparser.cpp \
    ../src/protocolconfig.cpp

HEADERS += \
    ../src/frameparser.h \
    ../src/protocolconfig.h

INCLUDEPATH += ../src

win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
