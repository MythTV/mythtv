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
HEADERS += ../mytharchive/archiveutil.h ../mytharchive/remoteavformatcontext.h external/pxsup2dast.h
SOURCES += mytharchivehelper.cpp ../mytharchive/archiveutil.cpp external/pxsup2dast.c

LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythpostproc
LIBS += -lmythavfilter
LIBS += -lz
LIBS += -lmythtv-$$LIBVERSION
# libmythtv dependencies
using_mheg: LIBS += -lmythfreemheg-$$LIBVERSION
using_hdhomerun: LIBS += -lhdhomerun

QT += xml sql opengl network widgets
