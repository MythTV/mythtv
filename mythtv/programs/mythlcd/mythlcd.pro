INCLUDEPATH += ../../libs/ ../../libs/libmyth

LIBS += -L../../libs/libmyth -L../../libs/libmythtv -L../../libs/libavcodec
LIBS += -L../../libs/libavformat -L../../libs/libvbitext

include (../../settings.pro)

TEMPLATE = app
CONFIG += thread

LIBS += -lmyth-$$LIBVERSION -lmythtv $$EXTRA_LIBS

DEPENDPATH += ../../libs/libmyth

# Input
HEADERS += mythlcd.h

SOURCES += main.cpp mythlcd.cpp
