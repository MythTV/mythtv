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

LIBS += -lmythswscale-$$LIBVERSION
LIBS += -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION
LIBS += -lmythavfilter-$$LIBVERSION
LIBS += -lz
LIBS += -lmythtv-$$LIBVERSION
# libmythtv dependencies
using_mheg: LIBS += -lmythfreemheg-$$LIBVERSION
using_hdhomerun: LIBS += -lhdhomerun

QT += xml sql opengl network widgets
