include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythfilerecorder.h
HEADERS += commandlineparser.h

SOURCES += main.cpp
SOURCES += commandlineparser.cpp

#The following line was inserted by qt3to4
QT += xml sql network

