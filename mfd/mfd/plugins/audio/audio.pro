include (../../../settings.pro)

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-audio

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          audio.h   
SOURCES += main.cpp audio.cpp 

HEADERS += ../../mfdplugin.h   ../../events.h
SOURCES += ../../mfdplugin.cpp ../../events.cpp

HEADERS += visual.h constants.h

HEADERS += output.h   output_event.h   recycler.h   buffer.h 
SOURCES += output.cpp output_event.cpp recycler.cpp buffer.cpp

HEADERS += decoder.h   decoder_event.h
SOURCES += decoder.cpp decoder_event.cpp

HEADERS += audiooutput.h   vorbisdecoder.h   maddecoder.h
SOURCES += audiooutput.cpp vorbisdecoder.cpp maddecoder.cpp

HEADERS += flacdecoder.h   cddecoder.h
SOURCES += flacdecoder.cpp cddecoder.cpp

LIBS += -logg -lvorbisfile -lvorbis
LIBS += -lmad -lid3tag -lcdaudio -lFLAC -lcdda_interface -lcdda_paranoia
