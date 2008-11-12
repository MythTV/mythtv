include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

QT += network xml sql qt3support

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfilldatabase
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += filldata.h   channeldata.h
HEADERS += icondata.h   xmltvparser.h
HEADERS += fillutil.h
SOURCES += filldata.cpp channeldata.cpp
SOURCES += icondata.cpp xmltvparser.cpp
SOURCES += fillutil.cpp
SOURCES += main.cpp
