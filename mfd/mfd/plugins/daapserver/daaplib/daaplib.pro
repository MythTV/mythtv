TEMPLATE = lib
CONFIG   = thread staticlib
QMAKE_CXXFLAGS_RELEASE = -fPIC -fno-common -Wno-multichar
QMAKE_CXXFLAGS_DEBUG += -fPIC -fno-common
TARGET = daaplib

HEADERS += basic.h  registry.h  taginput.h  tagoutput.h
SOURCES += registry.cpp  taginput.cpp  tagoutput.cpp


