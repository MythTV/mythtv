include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-audio

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          audio.h   
SOURCES += main.cpp audio.cpp 

HEADERS += audiooutput.h
SOURCES += audiooutput.cpp

LIBS += -logg -lvorbisfile -lvorbis -mfdlib
LIBS += -lmad -lid3tag -lcdaudio -lFLAC -lcdda_interface -lcdda_paranoia
