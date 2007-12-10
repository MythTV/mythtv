include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = app
CONFIG += thread opengl
TARGET = mtd
target.path = $${PREFIX}/bin
INSTALLS += target

LIBS += -lmythdvdnav-$$LIBVERSION

HEADERS += ../mythvideo/dbcheck.h logging.h mtd.h 
HEADERS += serversocket.h jobthread.h dvdprobe.h fileobs.h threadevents.h

SOURCES += main.cpp ../mythvideo/dbcheck.cpp logging.cpp mtd.cpp serversocket.cpp
SOURCES += jobthread.cpp dvdprobe.cpp fileobs.cpp threadevents.cpp

mingw:DEFINES += USING_MINGW
