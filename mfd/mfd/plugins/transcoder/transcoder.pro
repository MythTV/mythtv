include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread

TARGET = mfdplugin-transcoder

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          transcoder.h   job.h   importjob.h   audiocdjob.h   cdinput.h
SOURCES += main.cpp transcoder.cpp job.cpp importjob.cpp audiocdjob.cpp cdinput.cpp

HEADERS += encoder.h   vorbisencoder.h
SOURCES += encoder.cpp vorbisencoder.cpp

LIBS    += -lmad -lid3tag -logg -lvorbisfile -lvorbis -lvorbisenc -lcdaudio -lFLAC
LIBS    += -lmp3lame -lcdda_paranoia -lcdda_interface

