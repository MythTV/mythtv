include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
TARGET = mythuitest
CONFIG += thread opengl

LIBS += $$EXTRA_LIBS

macx {
    # Duplication of source with libmyth (e.g. oldsettings.cpp)
    # means that the linker complains, so we have to ignore duplicates 
    QMAKE_LFLAGS += -multiply_defined suppress
}

QMAKE_CLEAN += $(TARGET)
 
# Input
HEADERS = test1.h btnlisttest.h

SOURCES = main.cpp test1.cpp btnlisttest.cpp

