TEMPLATE = lib
CONFIG   = staticlib
QMAKE_CXXFLAGS_RELEASE = -Wno-multichar
TARGET = daaplib

HEADERS += basic.h  registry.h  taginput.h  tagoutput.h
SOURCES += registry.cpp  taginput.cpp  tagoutput.cpp


