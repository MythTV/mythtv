include (../settings.pro)

include (../options.pro)

TEMPLATE = lib
CONFIG += thread dll
TARGET = mtcp

target.path = $${PREFIX}/lib
INSTALLS += target

HEADERS += markupcodes.h   mtcpoutput.h   mtcpinput.h
SOURCES += markupcodes.cpp mtcpoutput.cpp mtcpinput.cpp

