include ( ../../settings.pro )

INCLUDEPATH += ../libmythtv

TEMPLATE = lib
TARGET = mythhdhomerun-$$LIBVERSION
CONFIG += thread dll
CONFIG -= qt
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += hdhomerun.h  hdhomerun_os.h  hdhomerun_sock.h  hdhomerun_types.h

HEADERS += hdhomerun_channels.h  hdhomerun_channelscan.h  hdhomerun_control.h
HEADERS += hdhomerun_debug.h     hdhomerun_device.h       hdhomerun_device_selector.h
HEADERS += hdhomerun_discover.h  hdhomerun_pkt.h          hdhomerun_video.h 

SOURCES += hdhomerun_channels.c  hdhomerun_channelscan.c  hdhomerun_control.c
SOURCES += hdhomerun_debug.c     hdhomerun_device.c       hdhomerun_device_selector.c
SOURCES += hdhomerun_discover.c  hdhomerun_pkt.c          hdhomerun_video.c

unix {
    HEADERS += hdhomerun_os_posix.h
    SOURCES += hdhomerun_os_posix.c hdhomerun_sock_posix.c
}

mingw {
    HEADERS += hdhomerun_os_windows.h
    SOURCES += hdhomerun_os_windows.c hdhomerun_sock_windows.c
    SOURCES -= hdhomerun_os_posix.c
    LIBS += -lws2_32 -liphlpapi -lpthread
}

LIBS += $$EXTRALIBS 

include ( ../libs-targetfix.pro )
