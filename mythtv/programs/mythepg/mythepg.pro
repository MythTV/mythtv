INCLUDEPATH += ../../libs/ ../../libs/libmyth
LIBS += -L../../libs/libmyth -L../../libs/libmythtv -L../../libs/libavcodec
LIBS += -L../../libs/libvbitext -L../../libs/libavformat

include (../../settings.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmythtv -lavformat -lavcodec -lvbitext -lmyth-$$LIBVERSION 
LIBS += $$EXTRA_LIBS -lmp3lame

DEPENDPATH += ../../libs/libmyth ../../libs/libmythtv

TARGETDEPS += ../../libs/libmythtv/libmythtv.a
TARGETDEPS += ../../libs/libavcodec/libavcodec.a
TARGETDEPS += ../../libs/libvbitext/libvbitext.a
TARGETDEPS += ../../libs/libavformat/libavformat.a

# Input
SOURCES += main.cpp
