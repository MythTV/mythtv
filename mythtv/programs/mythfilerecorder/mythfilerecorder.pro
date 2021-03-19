include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

QT += network xml sql widgets
contains(QT_MAJOR_VERSION, 5): QT += script

TEMPLATE = app
CONFIG += thread
target.files = mythfilerecorder
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythfilerecorder.h
HEADERS += commandlineparser.h

SOURCES += mythfilerecorder.cpp
SOURCES += commandlineparser.cpp

QT += xml sql network

