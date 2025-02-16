include ( ../../settings.pro )

QT += network sql widgets

TEMPLATE = lib
TARGET = mythprotoserver-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += PROTOSERVER_API

QMAKE_CLEAN += $(TARGET)

HEADERS += mythprotoserverexp.h
HEADERS += mythsocketmanager.h socketrequesthandler.h sockethandler.h

HEADERS += requesthandler/basehandler.h requesthandler/outboundhandler.h

HEADERS += requesthandler/fileserverhandler.h requesthandler/deletethread.h
HEADERS += requesthandler/fileserverutil.h sockethandler/filetransfer.h
HEADERS += requesthandler/messagehandler.h

SOURCES += mythsocketmanager.cpp sockethandler.cpp
SOURCES += requesthandler/basehandler.cpp requesthandler/outboundhandler.cpp

SOURCES += requesthandler/fileserverhandler.cpp requesthandler/deletethread.cpp
SOURCES += requesthandler/fileserverutil.cpp sockethandler/filetransfer.cpp
SOURCES += requesthandler/messagehandler.cpp

INCLUDEPATH += .. ../../external/FFmpeg

LIBS += -L../libmythbase -L../libmyth -L../libmythtv -L../libmythui
LIBS += -L../libmythupnp
LIBS += -L../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../external/FFmpeg/libavutil -L../../external/FFmpeg/libavcodec
LIBS += -L../../external/FFmpeg/libavformat -L../../external/FFmpeg/libswscale
LIBS += -lmythbase-$$LIBVERSION -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION -lmythupnp-$$LIBVERSION
LIBS += -lmythavutil -lmythavcodec -lmythavformat -lmythswscale
LIBS += $$EXTRA_LIBS $$LATE_LIBS $$QMAKE_LIBS_DYNLOAD
using_mheg:LIBS += -L../libmythfreemheg -lmythfreemheg-$$LIBVERSION

inc.path = $${PREFIX}/include/mythtv/libmythprotoserver

inc.files = $HEADERS

INSTALLS += inc

include ( ../libs-targetfix.pro )
