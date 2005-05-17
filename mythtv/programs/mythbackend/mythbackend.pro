INCLUDEPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs

LIBS += -L../../libs/libmyth -L../../libs/libmythtv -L../../libs/libavcodec
LIBS += -L../../libs/libavformat

include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythbackend
target.path = $${PREFIX}/bin
INSTALLS = target

DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libavcodec
DEPENDPATH += ../../libs/libavformat

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION 
LIBS += -lmythavcodec-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS
isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}
TARGETDEPS += ../../libs/libmyth/libmyth-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libmythtv/libmythtv-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libavformat/libmythavformat-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

# Input
HEADERS += autoexpire.h encoderlink.h filetransfer.h httpstatus.h mainserver.h
HEADERS += playbacksock.h scheduler.h server.h housekeeper.h

SOURCES += autoexpire.cpp encoderlink.cpp filetransfer.cpp httpstatus.cpp
SOURCES += main.cpp mainserver.cpp playbacksock.cpp scheduler.cpp server.cpp
SOURCES += housekeeper.cpp

using_oss:DEFINES += USING_OSS

using_dvb:DEFINES += USING_DVB
