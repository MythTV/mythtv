include ( ../../config.mak )
include ( ../../settings.pro )
 
TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += audiooutput.h audiooutputbase.h audiooutputnull.h
HEADERS += audiooutputdigitalencoder.h
HEADERS += backendselect.h dbsettings.h dialogbox.h
HEADERS += DisplayRes.h DisplayResScreen.h exitcodes.h
HEADERS += generictree.h httpcomms.h langsettings.h lcddevice.h
HEADERS += managedlist.h mythconfigdialogs.h mythconfiggroups.h
HEADERS += mythcontext.h mythdbcon.h mythdeque.h mythdialogs.h
HEADERS += mythevent.h mythexp.h mythmedia.h mythmediamonitor.h
HEADERS += mythobservable.h mythplugin.h mythpluginapi.h mythsocket.h
HEADERS += mythstorage.h mythwidgets.h mythwizard.h
HEADERS += oldsettings.h output.h qmdcodec.h remotefile.h
HEADERS += screensaver.h screensaver-null.h settings.h themeinfo.h
HEADERS += uilistbtntype.h uitypes.h util.h util-x11.h
HEADERS += volumebase.h volumecontrol.h virtualkeyboard.h visual.h xmlparse.h
HEADERS += mythhdd.h mythcdrom.h storagegroup.h dbutil.h
HEADERS += compat.h

SOURCES += audiooutput.cpp audiooutputbase.cpp audiooutputnull.cpp
SOURCES += audiooutputdigitalencoder.cpp
SOURCES += backendselect.cpp dbsettings.cpp dialogbox.cpp
SOURCES += DisplayRes.cpp DisplayResScreen.cpp
SOURCES += generictree.cpp httpcomms.cpp langsettings.cpp lcddevice.cpp
SOURCES += managedlist.cpp mythconfigdialogs.cpp mythconfiggroups.cpp
SOURCES += mythcontext.cpp mythdbcon.cpp mythdialogs.cpp
SOURCES += mythmedia.cpp mythmediamonitor.cpp
SOURCES += mythobservable.cpp mythplugin.cpp mythsocket.cpp
SOURCES += mythstorage.cpp mythwidgets.cpp mythwizard.cpp
SOURCES += oldsettings.cpp output.cpp qmdcodec.cpp remotefile.cpp
SOURCES += screensaver.cpp screensaver-null.cpp settings.cpp themeinfo.cpp
SOURCES += uilistbtntype.cpp uitypes.cpp util.cpp util-x11.cpp
SOURCES += volumebase.cpp volumecontrol.cpp virtualkeyboard.cpp xmlparse.cpp
SOURCES += mythhdd.cpp mythcdrom.cpp storagegroup.cpp dbutil.cpp

INCLUDEPATH += ../libmythsamplerate ../libmythsoundtouch ../libmythfreesurround
INCLUDEPATH += ../libavcodec ../libavutil
INCLUDEPATH += ../.. ../ ./
DEPENDPATH += ../libmythsamplerate ../libmythsoundtouch
DEPENDPATH += ../libmythfreesurround
DEPENDPATH += ../libavcodec ../libavutil
DEPENDPATH += ../ ../libmythui
DEPENDPATH += ../libmythupnp


LIBS += -L../libmythsamplerate   -lmythsamplerate-$${LIBVERSION}
LIBS += -L../libmythsoundtouch   -lmythsoundtouch-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}
LIBS += -L../libmythfreesurround -lmythfreesurround-$${LIBVERSION}
LIBS += -L../libavcodec          -lmythavcodec-$${LIBVERSION}
LIBS += -L../libavutil           -lmythavutil-$${LIBVERSION}

TARGETDEPS += ../libmythsamplerate/libmythsamplerate-$${MYTH_LIB_EXT}
TARGETDEPS += ../libmythsoundtouch/libmythsoundtouch-$${MYTH_LIB_EXT}
TARGETDEPS += ../libmythfreesurround/libmythfreesurround-$${MYTH_LIB_EXT}

# Install headers so that plugins can compile independently
inc.path = $${PREFIX}/include/mythtv/
inc.files  = dialogbox.h lcddevice.h mythcontext.h mythdbcon.h mythverbose.h
inc.files += mythwidgets.h remotefile.h util.h oldsettings.h volumecontrol.h
inc.files += settings.h uitypes.h xmlparse.h mythplugin.h mythdialogs.h
inc.files += audiooutput.h inetcomms.h httpcomms.h mythmedia.h mythwizard.h
inc.files += uilistbtntype.h generictree.h managedlist.h mythmediamonitor.h
inc.files += visual.h volumebase.h output.h langsettings.h qmdcodec.h
inc.files += exitcodes.h mythconfig.h mythconfig.mak virtualkeyboard.h
inc.files += mythevent.h mythobservable.h mythsocket.h
inc.files += mythexp.h mythpluginapi.h storagegroup.h compat.h
inc.files += mythstorage.h mythconfigdialogs.h mythconfiggroups.h

# Allow both #include <blah.h> and #include <libmyth/blah.h>
inc2.path  = $${PREFIX}/include/mythtv/libmyth
inc2.files = $${inc.files}


DEFINES += RUNPREFIX=\"$${RUNPREFIX}\"
DEFINES += LIBDIRNAME=\"$${LIBDIRNAME}\"


using_oss {
    DEFINES += USING_OSS
    SOURCES += audiooutputoss.cpp
    HEADERS += audiooutputoss.h
}

unix:!cygwin {
    SOURCES += mediamonitor-unix.cpp
    HEADERS += mediamonitor-unix.h
}

cygwin {
    QMAKE_LFLAGS_SHLIB += -Wl,--noinhibit-exec
    DEFINES += _WIN32
    #HEADERS += mediamonitor-windows.h
    #SOURCES += mediamonitor-windows.cpp
}

mingw {
    SOURCES += mediamonitor-windows.cpp audiooutputwin.cpp
    HEADERS += mediamonitor-windows.h   audiooutputwin.h

    LIBS += -lpthread

    LIBS -= -lmythui-$$LIBVERSION  -lmythupnp-$$LIBVERSION 
    LIBS += -L. -lmythui-bootstrap -lmythupnp-bootstrap
    POST_TARGETDEPS += libmythui-bootstrap.a libmythupnp-bootstrap.a
    implib.target    = libmythui-bootstrap.a
    implib.depends   = ../libmythui/libmythui.def
    implib.commands  = dlltool --input-def $$implib.depends   \
                       --dllname libmythui-$${LIBVERSION}.dll \
                       --output-lib $$implib.target  -k
    implib2.target   = libmythupnp-bootstrap.a
    implib2.depends  = ../libmythupnp/libmythupnp.def
    implib2.commands = dlltool --input-def $$implib2.depends    \
                       --dllname libmythupnp-$${LIBVERSION}.dll \
                       --output-lib $$implib2.target  -k
    QMAKE_EXTRA_WIN_TARGETS += implib implib2
}

macx {
    HEADERS += audiooutputca.h   screensaver-osx.h   DisplayResOSX.h
    SOURCES += audiooutputca.cpp screensaver-osx.cpp DisplayResOSX.cpp
    HEADERS += util-osx.h
    SOURCES += util-osx.cpp

    darwin_da {
        HEADERS += mediamonitor-darwin.h
        SOURCES += mediamonitor-darwin.cpp
        DEFINES += USING_DARWIN_DA
    }

    using_appleremote {
        HEADERS += AppleRemote.h   AppleRemoteListener.h
        SOURCES += AppleRemote.cpp AppleRemoteListener.cpp
        !using_lirc: HEADERS += lircevent.h
        !using_lirc: SOURCES += lircevent.cpp
    }

    # Mac OS X Frameworks
    FWKS = ApplicationServices AudioUnit Carbon CoreAudio IOKit

    darwin_da : FWKS += DiskArbitration

    # The following trick is tidier, and shortens the command line, but it
    # depends on the shell expanding Csh-style braces. Luckily, Bash & Zsh do.
    FC = $$join(FWKS,",","{","}")

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")


    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC6000000
}

linux {
    SOURCES += mythcdrom-linux.cpp
    HEADERS += mythcdrom-linux.h
}

freebsd {
    SOURCES += mythcdrom-freebsd.cpp
    HEADERS += mythcdrom-freebsd.h
}

INSTALLS += inc inc2

using_alsa {
    DEFINES += USE_ALSA
    HEADERS += audiooutputalsa.h
    SOURCES += audiooutputalsa.cpp
    LIBS += $$ALSA_LIBS
}

using_joystick_menu {
    DEFINES += USE_JOYSTICK_MENU
    HEADERS += jsmenu.h jsmenuevent.h
    SOURCES += jsmenu.cpp jsmenuevent.cpp
}

using_lirc {
    DEFINES += USE_LIRC
    HEADERS += lirc.h lircevent.h
    SOURCES += lirc.cpp lircevent.cpp
    LIBS += $$LIRC_LIBS
}

using_arts {
    DEFINES += USE_ARTS
    HEADERS += audiooutputarts.h
    SOURCES += audiooutputarts.cpp
    LIBS += $$ARTS_LIBS
}

using_jack {
    DEFINES += USE_JACK
    HEADERS += bio2jack.h audiooutputjack.h
    SOURCES += bio2jack.c audiooutputjack.cpp
    LIBS += $$JACK_LIBS
}

using_directx {
    DEFINES += USING_DIRECTX
    HEADERS += audiooutputdx.h
    SOURCES += audiooutputdx.cpp
}

using_x11 {
    DEFINES += USING_X11
    HEADERS += screensaver-x11.h
    SOURCES += screensaver-x11.cpp
    LIBS += $$EXTRA_LIBS
}

using_xrandr {
    DEFINES += USING_XRANDR
    HEADERS += DisplayResX.h
    SOURCES += DisplayResX.cpp
}

contains( HAVE_MMX, yes ) {
    HEADERS += ../../libs/libavcodec/i386/mmx.h ../../libs/libavcodec/dsputil.h
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

contains( CONFIG_LIBA52, yes ) {
    LIBS += -la52
}

contains( CONFIG_LIBFFTW3, yes ) {
    LIBS += -lfftw3f
}

include ( ../libs-targetfix.pro )
