include ( ../../config.mak )
include ( ../../settings.pro )

INCLUDEPATH += ../libmythtv

TEMPLATE = lib
TARGET = mythhdhomerun-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += hdhomerun.h  hdhomerun_os.h  hdhomerun_types.h

HEADERS += hdhomerun_channels.h  hdhomerun_channelscan.h  hdhomerun_control.h
HEADERS += hdhomerun_debug.h     hdhomerun_device.h       hdhomerun_dhcp.h
HEADERS += hdhomerun_discover.h  hdhomerun_pkt.h          hdhomerun_video.h 

SOURCES += hdhomerun_channels.c  hdhomerun_channelscan.c  hdhomerun_control.c
SOURCES += hdhomerun_debug.c     hdhomerun_device.c       hdhomerun_dhcp.c
SOURCES += hdhomerun_discover.c  hdhomerun_pkt.c          hdhomerun_video.c

include ( ../libs-targetfix.pro )
