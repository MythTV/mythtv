INCLUDEPATH += ../../libs/ ../../libs/libmyth

LIBS += -L../../libs/libmyth -L../../libs/libmythtv -L../../libs/libavcodec
LIBS += -L../../libs/libavformat

include ( ../../config.mak )
include (../../settings.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS

DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libavcodec
DEPENDPATH += ../../libs/libavformat

# Input
SOURCES += main.cpp
