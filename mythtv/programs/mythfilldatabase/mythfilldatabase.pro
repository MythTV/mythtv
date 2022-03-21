include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql widgets

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfilldatabase
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += filldata.h   channeldata.h
HEADERS += xmltvparser.h
HEADERS += fillutil.h   mythfilldatabase_commandlineparser.h
SOURCES += filldata.cpp channeldata.cpp
SOURCES += xmltvparser.cpp fillutil.cpp
SOURCES += mythfilldatabase.cpp     mythfilldatabase_commandlineparser.cpp
