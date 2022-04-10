include ( ../../settings.pro)
include ( ../../version.pro)
include ( ../programs-libs.pro)

QT += sql network widgets

CONFIG -= opengl

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythjobqueue_commandlineparser.h

SOURCES += mythjobqueue.cpp mythjobqueue_commandlineparser.cpp
