include ( ../../settings.pro )

INCLUDEPATH += ../../libs/libmythui ../../libs/libmyth
DEPENDPATH += ../../libs/libmythui ../../libs/libmyth

LIBS += -L../../libs/libmythui -lmythui-$$LIBVERSION

TEMPLATE = app
TARGET = mythuitest
CONFIG += thread opengl

# Input
HEADERS = test1.h

SOURCES = main.cpp test1.cpp

