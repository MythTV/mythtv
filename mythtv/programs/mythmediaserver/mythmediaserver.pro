include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network sql

TEMPLATE = app
CONFIG += thread
TARGET = mythmediaserver
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input

SOURCES += main.cpp
