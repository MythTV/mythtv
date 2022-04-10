include (../../settings.pro)
include (../../version.pro)
include (../programs-libs.pro)

QT += sql network

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythshutdown_commandlineparser.h
SOURCES += mythshutdown.cpp mythshutdown_commandlineparser.cpp
