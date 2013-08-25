# Copyright 2011-2013 Johann Duscher (a.k.a. Jonny Dee). All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
#    1. Redistributions of source code must retain the above copyright notice, this list of
#       conditions and the following disclaimer.
#
#    2. Redistributions in binary form must reproduce the above copyright notice, this list
#       of conditions and the following disclaimer in the documentation and/or other materials
#       provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY JOHANN DUSCHER ''AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# The views and conclusions contained in the software and documentation are those of the
# authors and should not be interpreted as representing official policies, either expressed
# or implied, of Johann Duscher.

include ( ../../../settings.pro )

QT       += core

QT       -= gui

TARGET = mythnzmqt
target.path = $${LIBDIR}
#CONFIG   += console
#CONFIG   -= app_bundle
CONFIG += thread dll
INSTALLS = target

TEMPLATE = lib

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

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

LIBS += -lmythzmq
LIBS += $${LATE_LIBS}

INCLUDEPATH += \
    ../include \
    ../../zeromq/include

QMAKE_LIBDIR += \
    ../../zeromq/src/.libs

inc.files += ../include/nzmqt/nzmqt.hpp
inc.path  = $${PREFIX}/include/mythtv/nzmqt/

INSTALLS += inc
