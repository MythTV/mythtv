include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-audio

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          audio.h	  audiolistener.h   
SOURCES += main.cpp audio.cpp audiolistener.cpp

HEADERS += daapinput.h   ../daapclient/daaprequest.h   ../daapclient/server_types.h 
SOURCES += daapinput.cpp ../daapclient/daaprequest.cpp

LIBS += -logg -lvorbisfile -lvorbis -mfdlib
LIBS += -lmad -lid3tag -lcdaudio -lFLAC -lcdda_interface -lcdda_paranoia
