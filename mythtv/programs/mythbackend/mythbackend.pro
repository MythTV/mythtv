INCLUDEPATH += ../../libs/libmythtv ../../libs/libmythtv ../../libs

LIBS += -L../../libs/libmythtv -L../../libs/libavcodec -L../../libs/libmyth
LIBS += -L../../libs/libvbitext -L../../libs/libavformat

include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythbackend
target.path = $${PREFIX}/bin
INSTALLS = target

DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libavcodec
DEPENDPATH += ../../libs/libvbitext ../../libs/libavformat

LIBS += -lmythtv -lavformat -lavcodec -lvbitext -lmyth-$$LIBVERSION 
LIBS += -lmp3lame $$EXTRA_LIBS

TARGETDEPS += ../../libs/libmythtv/libmythtv.a
TARGETDEPS += ../../libs/libavcodec/libavcodec.a
TARGETDEPS += ../../libs/libvbitext/libvbitext.a
TARGETDEPS += ../../libs/libavformat/libavformat.a

using_dvb {
    LIBS += -ldvbdev
    LIBS += -L../../libs/libdvbdev
    TARGETDEPS += ../../libs/libdvbdev/libdvbdev.a
    DEPENDPATH += ../../libs/libdvbdev
}

# Input
HEADERS += encoderlink.h filetransfer.h httpstatus.h mainserver.h playbacksock.h
HEADERS += scheduler.h server.h transcoder.h

SOURCES += encoderlink.cpp filetransfer.cpp httpstatus.cpp main.cpp 
SOURCES += mainserver.cpp playbacksock.cpp scheduler.cpp server.cpp
SOURCES += transcoder.cpp
