include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

SDK = /opt/NVIDIA_GPU_Computing_SDK
INCLUDEPATH += $$SDK/shared/inc $$SDK/OpenCL/common/inc
LIBPATH += $$SDK/shared/lib $$SDK/shared/lib/linux
LIBPATH += $$SDK/OpenCL/common/lib

LIBS += -lOpenCL -loclUtil_x86_64 -lshrutil_x86_64

# Input
HEADERS += commandlineparser.h
HEADERS += openclinterface.h
HEADERS += packetqueue.h
HEADERS += resultslist.h
HEADERS += gpuplayer.h
HEADERS += queueconsumer.h audioconsumer.h videoconsumer.h

SOURCES += main.cpp commandlineparser.cpp
SOURCES += openclinterface.cpp
SOURCES += packetqueue.cpp
SOURCES += gpuplayer.cpp
SOURCES += queueconsumer.cpp audioconsumer.cpp videoconsumer.cpp

#The following line was inserted by qt3to4
QT += xml sql network

