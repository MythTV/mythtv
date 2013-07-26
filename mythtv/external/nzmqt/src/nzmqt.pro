include ( ../../../settings.pro )

QT       += core
QT       -= gui

TARGET = mythnzmqt
target.path = $${LIBDIR}
#CONFIG   += console
#CONFIG   -= app_bundle
CONFIG += thread dll
INSTALLS = target
DEFINES += NZMQT_API

TEMPLATE = lib

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

SOURCES += \
    main.cpp

HEADERS += \
    ../include/nzmqt/nzmqt.hpp \
    ../include/nzmqt/nzmqtexp.h \
    pubsub/PubSubServer.h \
    pubsub/PubSubClient.h \
    reqrep/ReqRepServer.h \
    reqrep/ReqRepClient.h \
    pushpull/PushPullWorker.h \
    pushpull/PushPullVentilator.h \
    pushpull/PushPullSink.h \
    NzmqtApp.h \
    common/Tools.h

win32-msvc* {
  LIBS += -llibzmq
} else {
  LIBS += -lmythzmq
}
LIBS += $${LATE_LIBS}

INCLUDEPATH += \
    ../include \
    ../../zeromq/include

!win32-msvc* {
  QMAKE_LIBDIR += ../../zeromq/src/.libs
}

inc.files += ../include/nzmqt/nzmqt.hpp
inc.path  = $${PREFIX}/include/

INSTALLS += inc
