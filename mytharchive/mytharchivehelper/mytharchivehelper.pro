include ( ../mythconfig.mak )
include ( ../settings.pro )

INCLUDEPATH += $${PREFIX}/include/mythtv

TEMPLATE = app
CONFIG += thread opengl
target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += ../mytharchive/archiveutil.h
SOURCES += main.cpp ../mytharchive/archiveutil.cpp

LIBS += -L$${PREFIX}/lib
LIBS += -lmyth-$$LIBVERSION -lmythtv-$$LIBVERSION 
LIBS += -lmythui-$$LIBVERSION -lmythfreemheg-$$LIBVERSION -lmythlivemedia-$$LIBVERSION
