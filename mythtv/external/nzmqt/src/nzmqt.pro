#-------------------------------------------------
#
# Project created by QtCreator 2010-10-15T17:00:35
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = nzmqt
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += \
    main.cpp

HEADERS += \
    ../include/nzmqt/nzmqt.hpp \
    pubsub/PubSubServer.h \
    pubsub/PubSubClient.h \
    reqrep/ReqRepServer.h \
    reqrep/ReqRepClient.h \
    pushpull/PushPullWorker.h \
    pushpull/PushPullVentilator.h \
    pushpull/PushPullSink.h \
    NzmqtApp.h \
    common/Tools.h

LIBS += -lzmq

INCLUDEPATH += \
    ../include \
    /opt/local/include

QMAKE_LIBDIR += \
    /opt/local/lib

OTHER_FILES += \
    ../README.markdown \
    ../COPYING \
    ../LICENSE.header
