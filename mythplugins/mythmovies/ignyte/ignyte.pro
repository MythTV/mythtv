include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = ignyte
target.path = $${PREFIX}/bin
INSTALLS += target

# Input
HEADERS += ignytegrabber.h mythsoap.h
SOURCES += main.cpp ignytegrabber.cpp mythsoap.cpp
