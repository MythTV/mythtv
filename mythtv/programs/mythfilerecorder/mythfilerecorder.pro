include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

QT += network xml sql widgets

TEMPLATE = app
CONFIG += thread
target.files = mythfilerecorder
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythfilerecorder.h
HEADERS += mythfilerecorder_commandlineparser.h

SOURCES += mythfilerecorder.cpp
SOURCES += mythfilerecorder_commandlineparser.cpp

QT += xml sql network

