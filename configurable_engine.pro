QT -= gui
QT += core

CONFIG += c++11 release plugin
TEMPLATE = lib
TARGET = ConfigurableEngine
DESTDIR = $$PWD/dist

SOURCES += \
    src/configurableengine.cpp \
    src/frameparser.cpp \
    src/protocolconfig.cpp

HEADERS += \
    src/configurableengine.h \
    src/dataengineinterface.h \
    src/frameparser.h \
    src/protocolconfig.h

INCLUDEPATH += src

win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
