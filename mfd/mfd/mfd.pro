include ( ../settings.pro )

INCLUDEPATH += ../mfdlib
DEPENDPATH += ../mfdlib
LIBS += -Wl,--export-dynamic $$EXTRA_LIBS -L../mfdlib/ -lmfdlib -Wl,-rpath,$${PREFIX}/lib

!exists(../config.pro ) {
    error(Missing config.pro: please run the configure script)
}

!exists(../options.pro ) {
    error(Missing options.pro: please run the configure script)
}

include (../options.pro)


TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS += target

TARGET = mfd

!isEmpty(USE_MYTH_LIB) {
LIBS += -lmyth-$$LIBVERSION 
}


HEADERS +=          mfd.h   pluginmanager.h   serversocket.h   logging.h   \
           metadata.h   mdcontainer.h   mdmonitor.h   mdserver.h
SOURCES += main.cpp mfd.cpp pluginmanager.cpp serversocket.cpp logging.cpp \
           metadata.cpp mdcontainer.cpp mdmonitor.cpp mdserver.cpp


