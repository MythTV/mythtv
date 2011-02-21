include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../../mythtv/version.pro )
include ( ../../programs-libs.pro )

INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libavutil
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libavcodec
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libavformat
INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/libswscale
DEPENDPATH *= $${INCLUDEPATH}

TEMPLATE = app
CONFIG += thread opengl

target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += ../mytharchive/archiveutil.h pxsup2dast.h
SOURCES += main.cpp ../mytharchive/archiveutil.cpp pxsup2dast.c

LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lz

QT += xml sql opengl
