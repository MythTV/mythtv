include ( ../settings.pro )

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}

INCLUDEPATH += ../mfdlib
DEPENDPATH += ../mfdlib

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS += target

TARGET = mfd
LIBS += -Wl,--export-dynamic -lmyth-$$LIBVERSION $$EXTRA_LIBS -L../mfdlib/ -l mfdlib -Wl,-rpath,/usr/local/lib


HEADERS +=          mfd.h   pluginmanager.h   serversocket.h   logging.h   \
           metadata.h   mdcontainer.h
SOURCES += main.cpp mfd.cpp pluginmanager.cpp serversocket.cpp logging.cpp \
           metadata.cpp mdcontainer.cpp


