INCLUDEPATH += ../../libs/libmythtv ../../libs/libmythtv ../../libs

LIBS += -L../../libs/libmythtv -L../../libs/libavcodec -L../../libs/libmyth
LIBS += -L../../libs/libvbitext -L../../libs/libavformat

include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythtv
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmythtv -lavformat -lavcodec -lvbitext -lmyth-$$LIBVERSION 
LIBS += $$EXTRA_LIBS -lmp3lame

TARGETDEPS += ../../libs/libmythtv/libmythtv.a
TARGETDEPS += ../../libs/libavcodec/libavcodec.a
TARGETDEPS += ../../libs/libvbitext/libvbitext.a
TARGETDEPS += ../../libs/libavformat/libavformat.a

DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libavcodec
DEPENDPATH += ../../libs/libvbitext ../../libs/libavformat

# Input
SOURCES += main.cpp
