include ( ../../mythconfig.mak )
include ( ../../settings.pro )

QT += xml network

TEMPLATE = app
CONFIG += thread
TARGET = ignyte
target.path = $${PREFIX}/bin
INSTALLS += target

LIBS *= $$LOCAL_LIBDIR_X11

# Input
HEADERS += ignytegrabber.h mythsoap.h
SOURCES += main.cpp ignytegrabber.cpp mythsoap.cpp
