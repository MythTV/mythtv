INCLUDEPATH += ../../libs/ ../../libs/libmyth

LIBS += -L../../libs/libmyth -L../../libs/libmythtv

include (../../settings.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += -lmythtv-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS

DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth

# Input
SOURCES += main.cpp
