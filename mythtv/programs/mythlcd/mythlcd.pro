INCLUDEPATH += ../../libs/ ../../libs/libmyth

LIBS += -L../../libs/libmyth -L../../libs/libmythtv -L../../libs/libavcodec
LIBS += -L../../libs/libavformat

include (../../settings.pro)

TEMPLATE = app
CONFIG += thread

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS

DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libavcodec
DEPENDPATH += ../../libs/libavformat

# Input
HEADERS += mythlcd.h

SOURCES += main.cpp mythlcd.cpp
