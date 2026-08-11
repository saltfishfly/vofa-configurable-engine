QT -= gui
QT += core

CONFIG += console c++11 release
CONFIG -= app_bundle
TEMPLATE = app
TARGET = plugin_smoke
DESTDIR = $$PWD/../dist

SOURCES += plugin_smoke.cpp
HEADERS += ../src/dataengineinterface.h
INCLUDEPATH += ../src

win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
