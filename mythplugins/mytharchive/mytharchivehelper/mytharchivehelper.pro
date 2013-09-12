include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../../mythtv/version.pro )
include ( ../../programs-libs.pro )

DEPENDPATH *= $${INCLUDEPATH}

TEMPLATE = app
CONFIG += thread opengl

target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += ../mytharchive/archiveutil.h ../mytharchive/remoteavformatcontext.h pxsup2dast.h
SOURCES += main.cpp ../mytharchive/archiveutil.cpp pxsup2dast.c

LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lz

QT += xml sql opengl network
