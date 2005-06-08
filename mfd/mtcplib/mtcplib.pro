include (../settings.pro)

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}

include (../options.pro)

TEMPLATE = lib
CONFIG += thread dll
TARGET = mtcp

target.path = $${PREFIX}/lib
INSTALLS += target

HEADERS += markupcodes.h   mtcpoutput.h   mtcpinput.h
SOURCES += markupcodes.cpp mtcpoutput.cpp mtcpinput.cpp

