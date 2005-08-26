include ( ../../config.mak )
include (../../settings.pro)
include (../programs-libs.pro)

TEMPLATE = app
CONFIG += thread

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythlcd.h

SOURCES += main.cpp mythlcd.cpp
