include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.17.0

# Input
HEADERS += dialogbox.h lcddevice.h mythcontext.h mythwidgets.h oldsettings.h  
HEADERS += remotefile.h settings.h themedmenu.h util.h mythwizard.h
HEADERS += volumecontrol.h uitypes.h xmlparse.h mythplugin.h mythdbcon.h
HEADERS += mythdialogs.h audiooutput.h inetcomms.h httpcomms.h mythmedia.h 
HEADERS += uilistbtntype.h uiphoneentry.h generictree.h screensaver.h
HEADERS += managedlist.h DisplayRes.h volumebase.h audiooutputbase.h
HEADERS += dbsettings.h screensaver-null.h output.h visual.h
HEADERS += langsettings.h

SOURCES += dialogbox.cpp lcddevice.cpp mythcontext.cpp mythwidgets.cpp 
SOURCES += oldsettings.cpp remotefile.cpp settings.cpp themedmenu.cpp
SOURCES += util.cpp mythwizard.cpp uitypes.cpp xmlparse.cpp
SOURCES += mythplugin.cpp mythdialogs.cpp audiooutput.cpp inetcomms.cpp 
SOURCES += httpcomms.cpp mythmedia.cpp uilistbtntype.cpp uiphoneentry.cpp
SOURCES += generictree.cpp managedlist.cpp DisplayRes.cpp
SOURCES += volumecontrol.cpp volumebase.cpp audiooutputbase.cpp
SOURCES += dbsettings.cpp screensaver.cpp screensaver-null.cpp output.cpp
SOURCES += langsettings.cpp mythdbcon.cpp

INCLUDEPATH += ../libmythsamplerate ../libmythsoundtouch ../..
DEPENDPATH += ../libmythsamplerate ../libmythsoundtouch

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
inc.files  = dialogbox.h lcddevice.h themedmenu.h mythcontext.h mythdbcon.h
inc.files += mythwidgets.h remotefile.h util.h oldsettings.h volumecontrol.h
inc.files += settings.h uitypes.h xmlparse.h mythplugin.h mythdialogs.h
inc.files += audiooutput.h inetcomms.h httpcomms.h mythmedia.h mythwizard.h
inc.files += uilistbtntype.h uiphoneentry.h generictree.h managedlist.h
inc.files += visual.h volumebase.h output.h langsettings.h

using_oss {
    DEFINES += USING_OSS
    SOURCES += audiooutputoss.cpp
    HEADERS += audiooutputoss.h
}

unix {
    SOURCES += mythcdrom.cpp mythmediamonitor.cpp
    HEADERS += mythcdrom.h   mythmediamonitor.h
    inc.files += mythcdrom.h mythmediamonitor.h
}

macx {
    # OS X specific audio layer
    SOURCES        += audiooutputca.cpp
    HEADERS        += audiooutputca.h
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/CoreAudio.framework/Frameworks
    LIBS           += -framework CoreAudio
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/AudioUnit.framework/Frameworks
    LIBS           += -framework AudioUnit

    HEADERS        += screensaver-osx.h   DisplayResOSX.h
    SOURCES        += screensaver-osx.cpp DisplayResOSX.cpp
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/ApplicationServices.framework/Frameworks
    LIBS           += -framework ApplicationServices

    # We use HIToolbox from Carbon to hide the menu bar
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/Carbon.framework/Frameworks
    LIBS           += -framework Carbon
    
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
    HEADERS += DisplayResX.h
    SOURCES += DisplayResX.cpp
}
