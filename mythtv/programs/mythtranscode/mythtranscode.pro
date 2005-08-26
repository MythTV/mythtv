include ( ../../config.mak )
include ( ../../settings.pro)
include ( ../programs-libs.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp transcode.cpp mpeg2trans.cpp
