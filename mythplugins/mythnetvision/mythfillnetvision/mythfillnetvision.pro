include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network xml sql

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfillnetvision
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp

include ( ../../libs-targetfix.pro )
