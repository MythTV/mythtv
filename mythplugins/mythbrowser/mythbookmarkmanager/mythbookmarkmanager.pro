include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythbookmarkmanager
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += bookmarkmanager.h
SOURCES += main.cpp bookmarkmanager.cpp
