
INCLUDEPATH += ../mfdlib
DEPENDPATH += ../mfdlib
LIBS += -Wl,--export-dynamic -L../mfdlib/ -lmfdlib -L../mdcaplib/ -lmdcap

include ( ../settings.pro )

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

LIBS += -lmyth-$$LIBVERSION 


!isEmpty(USE_WMA_AUDIO){
LIBS += -lmythavformat-$$LIBVERSION
LIBS += -lmythavcodec-$$LIBVERSION
}

LIBS += -Wl,-rpath,$${PREFIX}/lib

HEADERS +=          mfd.h   pluginmanager.h   serversocket.h   logging.h   \
           mdserver.h   dbcheck.h   mdcaprequest.h   mdcapsession.h \
           signalthread.h
SOURCES += main.cpp mfd.cpp pluginmanager.cpp serversocket.cpp logging.cpp \
           mdserver.cpp dbcheck.cpp mdcaprequest.cpp mdcapsession.cpp \
           signalthread.cpp


