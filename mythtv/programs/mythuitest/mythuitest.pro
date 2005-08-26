INCLUDEPATH += ../../libs/libmythui ../../libs/libmyth

LIBS += -L../../libs/libmyth -L../../libs/libmythui

include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = app
TARGET = mythuitest
CONFIG += thread opengl

LIBS += -lmythui-$$LIBVERSION -lmyth-$$LIBVERSION

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}
TARGETDEPS += ../../libs/libmythui/libmythui-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../../libs/libmyth/libmyth-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

macx {
    # Duplication of source with libmyth (e.g. oldsettings.cpp)
    # means that the linker complains, so we have to ignore duplicates 
    QMAKE_LFLAGS += -multiply_defined suppress
}

DEPENDPATH += ../../libs/libmythui ../../libs/libmyth

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS = test1.h btnlisttest.h

SOURCES = main.cpp test1.cpp btnlisttest.cpp

