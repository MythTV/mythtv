TEMPLATE = app
CONFIG += thread

INCLUDEPATH += ../libs/libmythtv ../libs ../libs/libmyth

LIBS += -L../libs/libmyth -L../libs/libmythtv -L../libs/libavcodec
LIBS += -L../libs/libavformat

include ( ../config.mak )
include ( ../settings.pro )

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION -lmyth-$$LIBVERSION $$EXTRA_LIBS

DEPENDPATH += ../libs/libmythtv ../libs/libmyth ../libs/libavcodec
DEPENDPATH += ../libs/libavformat

INCLUDEPATH += ../libs/libmythtv/dvbdev

# Input
HEADERS += backendsettings.h
SOURCES += main.cpp backendsettings.cpp

menu.path = $${PREFIX}/share/mythtv/
menu.files += setup.xml

INSTALLS += menu
