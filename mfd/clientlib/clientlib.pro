include (../settings.pro)

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}

include (../options.pro)

TEMPLATE = lib
CONFIG += thread dll
TARGET = mfdclient

target.path = $${PREFIX}/lib
INSTALLS = target

HEADERS += mfdinterface.h   discoverythread.h   mfdinstance.h
SOURCES += mfdinterface.cpp discoverythread.cpp mfdinstance.cpp

inc.path = $${PREFIX}/include/mfdclient/
inc.files  = mfdinterface.h mfdinstance.h

INSTALLS += inc

