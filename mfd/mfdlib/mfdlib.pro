include (../settings.pro)

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}

include (../options.pro)

INCLUDEPATH *= /usr/include/cdda

TEMPLATE = lib
CONFIG += thread dll
TARGET = mfdlib

target.path = $${PREFIX}/lib
INSTALLS += target

HEADERS += mfd_events.h   mfd_plugin.h   requestthread.h   clientsocket.h  
SOURCES += mfd_events.cpp mfd_plugin.cpp requestthread.cpp clientsocket.cpp

HEADERS += httpinrequest.h   httpoutrequest.h   httpoutresponse.h   httpinresponse.h
SOURCES += httpinrequest.cpp httpoutrequest.cpp httpoutresponse.cpp httpinresponse.cpp

HEADERS += httpheader.h   httpgetvar.h   httpserver.h
SOURCES += httpheader.cpp httpgetvar.cpp httpserver.cpp

HEADERS += decoder.h   decoder_event.h   visual.h constants.h 
SOURCES += decoder.cpp decoder_event.cpp 

HEADERS += buffer.h   service.h
SOURCES += buffer.cpp service.cpp

HEADERS += vorbisdecoder.h   maddecoder.h
SOURCES += vorbisdecoder.cpp maddecoder.cpp

HEADERS += flacdecoder.h   cddecoder.h
SOURCES += flacdecoder.cpp cddecoder.cpp

HEADERS += metadata.h   mdcontainer.h   settings.h
SOURCES += metadata.cpp mdcontainer.cpp settings.cpp

HEADERS += avfdecoder.h   wavdecoder.h   aacdecoder.h   mythdigest.h
SOURCES += avfdecoder.cpp wavdecoder.cpp aacdecoder.cpp mythdigest.cpp

LIBS += -logg -lvorbisfile -lvorbis  -lmad -lid3tag -lcdaudio \
        -lFLAC -lcdda_interface -lcdda_paranoia -lssl

!isEmpty(USE_AAC_AUDIO) {
LIBS += -lfaad -lmp4ff
}
