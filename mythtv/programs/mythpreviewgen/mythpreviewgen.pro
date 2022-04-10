include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network sql widgets

TEMPLATE = app
CONFIG += thread
TARGET = mythpreviewgen
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

HEADERS += mythpreviewgen_commandlineparser.h

SOURCES += mythpreviewgen.cpp mythpreviewgen_commandlineparser.cpp

using_oss:DEFINES += USING_OSS

using_dvb:DEFINES += USING_DVB

using_valgrind:DEFINES += USING_VALGRIND

