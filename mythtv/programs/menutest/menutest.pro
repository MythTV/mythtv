INCLUDEPATH += ../../libs/libmyth
LIBS +=  -L../../libs/libmyth

TEMPLATE = app
CONFIG += thread
TARGET = menutest

include ( ../../settings.pro )

LIBS += -lmyth-$$LIBVERSION $$EXTRA_LIBS

# Input
SOURCES += main.cpp
