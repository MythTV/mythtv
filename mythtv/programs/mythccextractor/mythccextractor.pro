include (../../settings.pro)
include ( ../programs-libs.pro )

QT += sql network widgets

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += commandlineparser.h
SOURCES += commandlineparser.cpp mythccextractor.cpp

