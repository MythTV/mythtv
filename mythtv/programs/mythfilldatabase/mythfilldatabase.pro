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
SOURCES += filldata.cpp
