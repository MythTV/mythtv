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

# Input
HEADERS += autoexpire.h encoderlink.h filetransfer.h httpstatus.h mainserver.h
HEADERS += playbacksock.h scheduler.h server.h housekeeper.h

SOURCES += autoexpire.cpp encoderlink.cpp filetransfer.cpp httpstatus.cpp
SOURCES += main.cpp mainserver.cpp playbacksock.cpp scheduler.cpp server.cpp
SOURCES += housekeeper.cpp
