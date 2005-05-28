include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread

TARGET = mfdplugin-transcoder

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS +=          transcoder.h   job.h   importjob.h   audiocdjob.h
SOURCES += main.cpp transcoder.cpp job.cpp importjob.cpp audiocdjob.cpp

#	LIBS += -L./daaplib/ -ldaaplib -lid3tag -lz 

