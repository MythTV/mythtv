include (../settings.pro)

include (../options.pro)

TEMPLATE = lib
CONFIG += thread dll
TARGET = mdcap

target.path = $${PREFIX}/lib
INSTALLS += target

HEADERS += markupcodes.h   mdcapoutput.h   mdcapinput.h
SOURCES += markupcodes.cpp mdcapoutput.cpp mdcapinput.cpp

