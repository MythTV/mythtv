include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythtvosd
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += $$EXTRA_LIBS 
INCLUDEPATH += ../../libs/libmythdb

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp

mingw: LIBS += -lpthread -lwinmm -lws2_32

LIBS -= -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS -= -lmythavutil-$$LIBVERSION -lmythavcodec-$$LIBVERSION 
LIBS -= -lmythfreemheg-$$LIBVERSION
LIBS -= -lmythupnp-$$LIBVERSION 
LIBS -= -lmythlivemedia-$$LIBVERSION
LIBS -= -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION $$EXTRA_LIBS
LIBS -= -lmythdb-$$LIBVERSION

#The following line was inserted by qt3to4
QT += network qt3support
