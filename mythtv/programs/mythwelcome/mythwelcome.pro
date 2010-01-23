include (../../settings.pro)
include (../programs-libs.pro)

QT += xml sql

TEMPLATE = app
CONFIG += thread
TARGET = mythwelcome
target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += welcomedialog.h welcomesettings.h
SOURCES += main.cpp welcomedialog.cpp welcomesettings.cpp
