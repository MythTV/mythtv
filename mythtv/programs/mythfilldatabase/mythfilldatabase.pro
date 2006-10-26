include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfilldatabase
target.path = $${PREFIX}/bin
INSTALLS = target

INCLUDEPATH += ../../libs/libmyth ./../libs/libmythtv
DEPENDPATH  += ../../libs/libmyth ./../libs/libmythtv

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += filldata.h   channeldata.h
HEADERS += icondata.h   xmltvparser.h
HEADERS += fillutil.h
SOURCES += filldata.cpp channeldata.cpp
SOURCES += icondata.cpp xmltvparser.cpp
SOURCES += fillutil.cpp
SOURCES += main.cpp
