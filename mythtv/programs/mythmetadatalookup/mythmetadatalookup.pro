include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql widgets

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythmetadatalookup
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += lookup.h mythmetadatalookup_commandlineparser.h
SOURCES += mythmetadatalookup.cpp lookup.cpp mythmetadatalookup_commandlineparser.cpp

