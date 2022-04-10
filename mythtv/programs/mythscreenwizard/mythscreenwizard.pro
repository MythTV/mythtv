include ( ../../settings.pro)
include ( ../../version.pro)
include ( ../programs-libs.pro)

QT += sql network widgets

TEMPLATE = app
CONFIG += thread
TARGET = mythscreenwizard
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythscreenwizard_commandlineparser.h screenwizard.h

SOURCES += mythscreenwizard.cpp mythscreenwizard_commandlineparser.cpp screenwizard.cpp

using_x11:DEFINES += USING_X11
