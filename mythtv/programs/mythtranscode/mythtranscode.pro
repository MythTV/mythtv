INCLUDEPATH += ../../libs/ ../../libs/libmyth ../../libs/libmythtv \
               ../../libs/libavcodec ../../libs/libavformat

LIBS += -L../../libs/libmyth -L../../libs/libmythtv -L../../libs/libavcodec
LIBS += -L../../libs/libavformat

include ( ../../config.mak )
include (../../settings.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libavcodec
DEPENDPATH += ../../libs/libavformat

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS

# Input
SOURCES += main.cpp transcode.cpp mpeg2trans.cpp
