include ( ../settings.pro )

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}


TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS += target

TARGET = mfd
LIBS += -lmyth-$$LIBVERSION $$EXTRA_LIBS
HEADERS +=          mfd.h   pluginmanager.h   serversocket.h   logging.h   events.h
SOURCES += main.cpp mfd.cpp pluginmanager.cpp serversocket.cpp logging.cpp events.cpp


