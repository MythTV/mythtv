#CONFIG += debug
CONFIG += release

PREFIX = /usr/local

LIBVERSION = 0.14

INCLUDEPATH += $${PREFIX}/include
INCLUDEPATH *= /usr/local/include

DEFINES += _GNU_SOURCE
DEFINES += _FILE_OFFSET_BITS=64
DEFINES += PREFIX=\"$${PREFIX}\"

release {
        DEFINES += MMX
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
        QMAKE_CFLAGS_RELEASE = $${QMAKE_CXXFLAGS_RELEASE}
}

EXTRA_LIBS = -lfreetype -lmp3lame

# Default Xv support
CONFIG += using_xv
EXTRA_LIBS += -L/usr/X11R6/lib -lXinerama -lXv -lX11 -lXext -lXxf86vm

# LCDProc support
#DEFINES += LCD_DEVICE

# Native ALSA support
#CONFIG += using_alsa
#ALSA_LIBS = -lasound

# Native ARTS support
#CONFIG += using_arts
#ARTS_LIBS = -L/opt/kde3/lib -ldl -lartsc -lpthread
#EXTRA_LIBS += -L/opt/kde3/lib -ldl -lartsc -lpthread
#INCLUDEPATH += /opt/kde3/include

# DVB support
#CONFIG += using_dvb
#DEFINES += USING_DVB
# Note: INCLUDEPATH should point to the directory with
#   'linux/dvb/frontend.h', not the directory with frontend.h
#INCLUDEPATH += /usr/src/linuxtv-dvb-1.0.1/include

# Native lirc support
#CONFIG += using_lirc
#LIRC_LIBS = -llirc_client

# XvMC support, modify as necessary.
#CONFIG += using_xvmc
#DEFINES += USING_XVMC
#EXTRA_LIBS += -lXvMCNVIDIA -lXvMC

# VIA cle266 support
#CONFIG += using_viahwslice
#DEFINES += USING_VIASLICE
#EXTRA_LIBS += -lddmpeg

# DirectFB support
#CONFIG += using_directfb
#EXTRA_LIBS += `directfb-config --libs`
#QMAKE_CXXFLAGS += `directfb-config --cflags`

