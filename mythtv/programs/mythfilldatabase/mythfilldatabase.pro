include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfilldatabase
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += filldata.cpp
