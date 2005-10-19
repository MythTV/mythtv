include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( config.pro )

!exists( ../mythdvd/config.pro ) {
    error(Missing config.pro: please run the configure script)
}

TEMPLATE = app
CONFIG += thread
TARGET = mtd
target.path = $${PREFIX}/bin
INSTALLS += target

LIBS += -lmyth-$$LIBVERSION $$EXTRA_LIBS

HEADERS += ../mythdvd/config.h ../mythdvd/dbcheck.h

SOURCES += main.cpp ../mythdvd/dbcheck.cpp

