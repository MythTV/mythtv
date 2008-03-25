include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = ignyte
target.path = $${PREFIX}/bin
INSTALLS += target

LIBS *= $$LOCAL_LIBDIR_X11

# Input
HEADERS += ignytegrabber.h mythsoap.h
SOURCES += main.cpp ignytegrabber.cpp mythsoap.cpp
#The following line was inserted by qt3to4
QT += xml  qt3support 
