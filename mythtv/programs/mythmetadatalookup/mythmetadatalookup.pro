include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythmetadatalookup
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += lookup.h commandlineparser.h
SOURCES += mythmetadatalookup.cpp lookup.cpp commandlineparser.cpp

