#CONFIG += debug
CONFIG += release

isEmpty( PREFIX ) {
    PREFIX = /usr/local
}

LIBVERSION = 0.16

INCLUDEPATH += $${PREFIX}/include

DEFINES += _GNU_SOURCE
DEFINES += _FILE_OFFSET_BITS=64
DEFINES += PREFIX=\"$${PREFIX}\"

release {
        DEFINES += MMX
        QMAKE_CXXFLAGS_RELEASE = -O3 -march=pentiumpro -fomit-frame-pointer
    macx {
        DEFINES -= MMX
        # Don't use -O3, it causes some Qt moc methods to go missing '
        QMAKE_CXXFLAGS_RELEASE = -O2
    }
    QMAKE_CFLAGS_RELEASE = $${QMAKE_CXXFLAGS_RELEASE}
}

EXTRA_LIBS = -lfreetype -lmp3lame

unix:linux*: {
    CONFIG  += linux backend
    DEFINES += CONFIG_VIDEO4LINUX
}
unix:freebsd*: {
    CONFIG  += freebsd backend
    DEFINES += CONFIG_VIDEO4LINUX
}

# X11 support
CONFIG += using_x11

# Default Xv support
CONFIG += using_xv
EXTRA_LIBS += -L/usr/X11R6/lib -lXinerama -lXv -lX11 -lXext -lXxf86vm

# IVTV (PVR-x50) support
CONFIG += using_ivtv
DEFINES += USING_IVTV

# LCDProc support
#DEFINES += LCD_DEVICE

# Default audio output, OSS
CONFIG += using_oss
DEFINES += USING_OSS

# Native ALSA support
#CONFIG += using_alsa
#ALSA_LIBS = -lasound

# Native ARTS support
#CONFIG += using_arts
#ARTS_LIBS = -L/opt/kde3/lib -ldl -lartsc -lpthread
#EXTRA_LIBS += -L/opt/kde3/lib -ldl -lartsc -lpthread
#INCLUDEPATH += /opt/kde3/include
# For Mandrake, use the following:
#ARTS_LIBS = -ldl -lartsc -lpthread
#EXTRA_LIBS += -ldl -lartsc -lpthread
#INCLUDEPATH += /usr/include/artsc

# Native JACK support
#CONFIG += using_jack
#JACK_LIBS += -ljack

# DVB support
#CONFIG += using_dvb
#DEFINES += USING_DVB
# Note: INCLUDEPATH should point to the directory with
#   'linux/dvb/frontend.h', not the directory with frontend.h
#INCLUDEPATH += /usr/src/linuxtv-dvb-1.0.1/include
#define the following if you want On Air Guide information
#DEFINES += USING_DVB_EIT

# Joystick menu support
CONFIG += using_joystick_menu

# Native lirc support
#CONFIG += using_lirc
#LIRC_LIBS = -llirc_client

# XvMC support, modify as necessary.
#CONFIG += using_xvmc
#DEFINES += USING_XVMC
#EXTRA_LIBS += -lXvMCNVIDIA -lXvMC

# XvMC_VLD support, modify as necessary. Incompatible with normal XvMC support.
#CONFIG += using_xvmc using_xvmc_vld
#DEFINES += USING_XVMC USING_XVMC_VLD
#EXTRA_LIBS += -lviaXvMC -lXvMC

# DirectFB support
#CONFIG += using_directfb
#EXTRA_LIBS += `directfb-config --libs`
#QMAKE_CXXFLAGS += `directfb-config --cflags`

# Windows support
win32 {
    DEFINES += _WIN32
}

# Mac OS X support
macx {
    # For source that uses Qt, Q_OS_MACX is defined.
    # For source that uses config.h, CONFIG_DARWIN is defined.
    # For other source (e.g. libmythtv), we define this
    DEFINES += CONFIG_DARWIN

    # This .pro file doesn't include config.mak. Force processor architecture:
    TARGET_ARCH_POWERPC=yes
    # Also force Altivec support:
    TARGET_ALTIVEC=yes
}

contains( TARGET_ALTIVEC, yes ) {
    # Altivec support
    CONFIG  += using_altivec
    # The USING_ALTIVEC define is deprecated; Altivec code should 
    # include "config.h" instead.
    macx {
        QMAKE_CFLAGS   += -faltivec
        QMAKE_CXXFLAGS += -faltivec
    }
    !macx {
        QMAKE_CFLAGS   += -maltivec -mabi=altivec
        QMAKE_CXXFLAGS += -maltivec -mabi=altivec
    }
}

# DirectX support
#CONFIG += using_directx
#DEFINES += USING_DIRECTX

# OpenGL support for vertical retrace sync
#DEFINES += USING_OPENGL_VSYNC
#EXTRA_LIBS += -lGL -lGLU
#CONFIG += using_opengl

# Allow use of XrandR to change display resolutions
#CONFIG += using_xrandr
#DEFINES += USING_XRANDR

################################################################
# Please keep these CONFIG blocks as the last part of this file!
# Add any new features (e.g. CONFIG+=, DEFINES+=) before them.
################################################################
# Disable the Linux defaults.
# Most developers will already know to hand-edit these out,
# but for the lazy ones who haven't, we will do it for them :-)
macx {
    CONFIG     -= using_x11
    CONFIG     -= using_xv
    EXTRA_LIBS -= -L/usr/X11R6/lib -lXinerama -lXv -lX11 -lXext -lXxf86vm
    CONFIG     -= using_ivtv
    DEFINES    -= USING_IVTV
    CONFIG     -= using_oss
    DEFINES    -= USING_OSS
    CONFIG     -= using_joystick_menu
}
win32 {
    CONFIG     -= using_x11
    CONFIG     -= using_xv
    EXTRA_LIBS -= -L/usr/X11R6/lib -lXinerama -lXv -lX11 -lXext -lXxf86vm
    CONFIG     -= using_ivtv
    DEFINES    -= USING_IVTV
    CONFIG     -= using_oss
    DEFINES    -= USING_OSS
    CONFIG     -= using_joystick_menu
}
freebsd {
    CONFIG     -= using_joystick_menu
}

################################################################
# Please keep these CONFIG blocks as the last part of this file!
# Add any new features (e.g. CONFIG+=, DEFINES+=) before them.
################################################################
