INCLUDEPATH += ../../libs/ ../../libs/libmyth

LIBS += -L../../libs/libavcodec -L../../libs/libavformat
LIBS += -L../../libs/libmyth -L../../libs/libmythtv

include ( ../../config.mak )
include (../../settings.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmythavcodec-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS

DEPENDPATH += ../../libs/libavcodec ../../libs/libavformat
DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth

# Input
SOURCES += main.cpp
