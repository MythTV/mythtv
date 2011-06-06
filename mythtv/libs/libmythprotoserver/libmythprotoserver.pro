include ( ../../settings.pro )

QT += network sql

TEMPLATE = lib
TARGET = mythprotoserver-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += PROTOSERVER_API

QMAKE_CLEAN += $(TARGET)

HEADERS += mythprotoserverexp.h
HEADERS += mythsocketmanager.h socketrequesthandler.h sockethandler.h

HEADERS += requesthandler/basehandler.h

HEADERS += requesthandler/fileserverhandler.h requesthandler/deletethread.h
HEADERS += requesthandler/fileserverutil.h sockethandler/filetransfer.h

SOURCES += mythsocketmanager.cpp sockethandler.cpp
SOURCES += requesthandler/basehandler.cpp

SOURCES += requesthandler/fileserverhandler.cpp requesthandler/deletethread.cpp
SOURCES += requesthandler/fileserverutil.cpp sockethandler/filetransfer.cpp

INCLUDEPATH += ../libmythbase ../libmyth ../libmythtv ../libmythui
INCLUDEPATH += ../../external/FFmpeg ../.. ../ ./

DEPENDPATH += ../ ../libmythbase ../libmythtv ../libmyth ../libmythui

LIBS += -L../libmythbase -L../libmyth -L../libmythtv -L../libmythui
LIBS += $$EXTRA_LIBS $$LATE_LIBS $$QMAKE_LIBS_DYNLOAD

inc.path = $${PREFIX}/include/mythtv/protoserver

inc.files = $HEADERS

INSTALLS += inc

include ( ../libs-targetfix.pro )
