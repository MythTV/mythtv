include ( ../../mythconfig.mak )
include (../../settings.pro )

TEMPLATE = app

CONFIG -= qt

TARGET = mythzmserver
target.path = $${PREFIX}/bin

INSTALLS = target

LIBS = $$system(mysql_config --libs)

# Input
HEADERS += zmserver.h

SOURCES += main.cpp zmserver.cpp
