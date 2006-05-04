include ( ../mythconfig.mak )
include ( ../settings.pro )

TEMP = $${SRC_PATH}
TEMP ~= s/'//

INCLUDEPATH += $${TEMP}/libs $${TEMP}/libs/libmythtv $${TEMP}/libs/libmyth
INCLUDEPATH += $${TEMP}/libs/libavcodec $${TEMP}/libs/libavutil

TEMPLATE = app
CONFIG += thread opengl
target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += 
SOURCES += main.cpp

LIBS += -L$${PREFIX}/lib
LIBS += -lmyth-$$LIBVERSION -lmythtv-$$LIBVERSION 
LIBS +=  -lmythui-$$LIBVERSION -lmythfreemheg-$$LIBVERSION
