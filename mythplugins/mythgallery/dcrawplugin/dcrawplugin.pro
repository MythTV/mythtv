include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin static
TARGET = dcrawplugin

# Input
HEADERS += config.h
HEADERS += dcrawformats.h
HEADERS += dcrawhandler.h
HEADERS += dcrawplugin.h
SOURCES += dcrawformats.cpp
SOURCES += dcrawhandler.cpp
SOURCES += dcrawplugin.cpp

