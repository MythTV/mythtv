include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network sql widgets

TEMPLATE = app
CONFIG += thread
TARGET = mythmediaserver
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += commandlineparser.h controlrequesthandler.h
SOURCES += main.cpp commandlineparser.cpp controlrequesthandler.cpp
