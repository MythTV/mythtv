include ( ../../settings.pro )

INCLUDEPATH += ../../libs/libmythui
DEPENDPATH += ../../libs/libmythui

LIBS += -L../../libs/libmythui -lmythui-$$LIBVERSION

TEMPLATE = app
TARGET = mythuitest
CONFIG += thread opengl

# Input
HEADERS = test1.h

SOURCES = main.cpp test1.cpp

