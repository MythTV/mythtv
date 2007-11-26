include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )


TEMPLATE = app
CONFIG += thread opengl

target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += ../mytharchive/archiveutil.h
SOURCES += main.cpp ../mytharchive/archiveutil.cpp

LIBS += -lmythtv-$$LIBVERSION
LIBS += -lmythfreemheg-$$LIBVERSION -lmythlivemedia-$$LIBVERSION
LIBS += -lmythavutil-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION
LIBS += -lmythavformat-$$LIBVERSION
