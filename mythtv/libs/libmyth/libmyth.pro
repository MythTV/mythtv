include ( ../../config.mak )
include ( ../../settings.pro )
 
TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += dialogbox.h lcddevice.h mythcontext.h mythwidgets.h oldsettings.h  
HEADERS += remotefile.h settings.h util.h mythwizard.h
HEADERS += volumecontrol.h uitypes.h xmlparse.h mythplugin.h mythdbcon.h
HEADERS += mythdialogs.h audiooutput.h httpcomms.h mythmedia.h 
HEADERS += uilistbtntype.h generictree.h screensaver.h
HEADERS += managedlist.h DisplayRes.h volumebase.h audiooutputbase.h
HEADERS += dbsettings.h screensaver-null.h output.h visual.h
HEADERS += langsettings.h audiooutputnull.h mythsocket.h
HEADERS += DisplayResScreen.h util-x11.h mythdeque.h qmdcodec.h
HEADERS += exitcodes.h virtualkeyboard.h mythobservable.h mythevent.h

SOURCES += dialogbox.cpp lcddevice.cpp mythcontext.cpp mythwidgets.cpp 
SOURCES += oldsettings.cpp remotefile.cpp settings.cpp
SOURCES += util.cpp mythwizard.cpp uitypes.cpp xmlparse.cpp
SOURCES += mythplugin.cpp mythdialogs.cpp audiooutput.cpp  
SOURCES += httpcomms.cpp mythmedia.cpp uilistbtntype.cpp 
SOURCES += generictree.cpp managedlist.cpp DisplayRes.cpp
SOURCES += volumecontrol.cpp volumebase.cpp audiooutputbase.cpp
SOURCES += dbsettings.cpp screensaver.cpp screensaver-null.cpp output.cpp
SOURCES += langsettings.cpp mythdbcon.cpp audiooutputnull.cpp
SOURCES += DisplayResScreen.cpp util-x11.cpp qmdcodec.cpp
SOURCES += virtualkeyboard.cpp mythobservable.cpp mythsocket.cpp

INCLUDEPATH += ../libmythsamplerate ../libmythsoundtouch ../.. ../
DEPENDPATH += ../libmythsamplerate ../libmythsoundtouch ../ ../libmythui

LIBS += -L../libmythsamplerate -lmythsamplerate-$${LIBVERSION}
LIBS += -L../libmythsoundtouch -lmythsoundtouch-$${LIBVERSION}

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}

TARGETDEPS += ../libmythsamplerate/libmythsamplerate-$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}
TARGETDEPS += ../libmythsoundtouch/libmythsoundtouch-$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}

inc.path = $${PREFIX}/include/mythtv/
inc.files  = dialogbox.h lcddevice.h mythcontext.h mythdbcon.h
inc.files += mythwidgets.h remotefile.h util.h oldsettings.h volumecontrol.h
inc.files += settings.h uitypes.h xmlparse.h mythplugin.h mythdialogs.h
inc.files += audiooutput.h inetcomms.h httpcomms.h mythmedia.h mythwizard.h
inc.files += uilistbtntype.h generictree.h managedlist.h
inc.files += visual.h volumebase.h output.h langsettings.h qmdcodec.h
inc.files += exitcodes.h mythconfig.h mythconfig.mak virtualkeyboard.h
inc.files += mythevent.h mythobservable.h mythsocket.h

using_oss {
    DEFINES += USING_OSS
    SOURCES += audiooutputoss.cpp
    HEADERS += audiooutputoss.h
}

unix {
    SOURCES += mythhdd.cpp mythcdrom.cpp mythmediamonitor.cpp
    HEADERS += mythhdd.h mythcdrom.h   mythmediamonitor.h
    inc.files += mythhdd.h mythcdrom.h mythmediamonitor.h
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
        HEADERS += AppleRemote.h   AppleRemoteListener.h   lircevent.h
        SOURCES += AppleRemote.cpp AppleRemoteListener.cpp lircevent.cpp
    }

    # Mac OS X Frameworks
    FWKS = ApplicationServices AudioUnit Carbon CoreAudio IOKit

    # The following trick is tidier, and shortens the command line, but it
    # depends on the shell expanding Csh-style braces. Luckily, Bash & Zsh do.
    FC = $$join(FWKS,",","{","}")

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")

    # There is a dependence on some stuff in libmythui.
    # It isn't built yet, so we have to ignore these for now:
    QMAKE_LFLAGS_SHLIB += -flat_namespace -undefined warning

    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC6000000
}

linux {
    SOURCES += mythcdrom-linux.cpp
}

freebsd {
    SOURCES += mythcdrom-freebsd.cpp
}

INSTALLS += inc

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
    HEADERS += audiooutputdx.h
    SOURCES += audiooutputdx.cpp
}

using_x11 {
    HEADERS += screensaver-x11.h
    SOURCES += screensaver-x11.cpp
    LIBS += $$EXTRA_LIBS
}

using_xrandr {
    DEFINES += USING_XRANDR
    HEADERS += DisplayResX.h
    SOURCES += DisplayResX.cpp
}

contains( TARGET_MMX, yes ) {
    HEADERS += ../../libs/libavcodec/i386/mmx.h ../../libs/libavcodec/dsputil.h
}
