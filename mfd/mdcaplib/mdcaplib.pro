include (../settings.pro)

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}

include (../options.pro)

TEMPLATE = lib
CONFIG += thread dll
TARGET = mdcap

target.path = $${PREFIX}/lib
INSTALLS += target

HEADERS += markupcodes.h   mdcapoutput.h
SOURCES += markupcodes.cpp mdcapoutput.cpp

