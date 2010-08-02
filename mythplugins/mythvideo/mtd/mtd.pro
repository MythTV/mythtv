include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += sql xml network

TEMPLATE = app
CONFIG += thread opengl
TARGET = mtd
target.path = $${PREFIX}/bin
INSTALLS += target

LIBS += -lmythdvdnav-$$LIBVERSION -ldl

HEADERS += ../mythvideo/dbcheck.h logging.h mtd.h 
HEADERS += jobthread.h dvdprobe.h fileobs.h threadevents.h

SOURCES += main.cpp ../mythvideo/dbcheck.cpp logging.cpp mtd.cpp
SOURCES += jobthread.cpp dvdprobe.cpp fileobs.cpp threadevents.cpp

mingw:DEFINES += USING_MINGW

win32:LIBS += -lws2_32
macx :LIBS += -framework CoreFoundation
