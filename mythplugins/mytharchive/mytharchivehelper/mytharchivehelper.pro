include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )


TEMPLATE = app
CONFIG += thread opengl

target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += ../mytharchive/archiveutil.h pxsup2dast.h
SOURCES += main.cpp ../mytharchive/archiveutil.cpp pxsup2dast.c

LIBS += -lz
LIBS += -lmythtv-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION
LIBS += -lmythavformat-$$LIBVERSION

using_live: LIBS += -lmythlivemedia-$$LIBVERSION
using_mheg: LIBS += -lmythfreemheg-$$LIBVERSION

QT += xml sql opengl
