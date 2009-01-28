include ( ../../config.mak )
include ( ../../settings.pro )

INCLUDEPATH += ../libmythtv

TEMPLATE = lib
TARGET = mythhdhomerun-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += hdhomerun_channelscan.h  hdhomerun_debug.h     hdhomerun_discover.h
HEADERS += hdhomerun_pkt.h          hdhomerun_channels.h  hdhomerun_device.h
HEADERS += hdhomerun.h              hdhomerun_types.h     hdhomerun_control.h
HEADERS += hdhomerun_dhcp.h         hdhomerun_os.h        hdhomerun_video.h

SOURCES += hdhomerun_channels.c     hdhomerun_config.c    hdhomerun_debug.c
SOURCES += hdhomerun_dhcp.c         hdhomerun_pkt.c       hdhomerun_channelscan.c
SOURCES += hdhomerun_control.c      hdhomerun_device.c    hdhomerun_discover.c
SOURCES += hdhomerun_video.c

include ( ../libs-targetfix.pro )
