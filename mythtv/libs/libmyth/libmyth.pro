include ( ../../settings.pro )

TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.15.0

# Input
HEADERS += dialogbox.h lcddevice.h mythcontext.h mythwidgets.h oldsettings.h  
HEADERS += remotefile.h settings.h themedmenu.h util.h mythwizard.h
HEADERS += volumecontrol.h uitypes.h xmlparse.h mythplugin.h mythdbcon.h
HEADERS += mythdialogs.h audiooutput.h inetcomms.h httpcomms.h mythmedia.h 
HEADERS += uilistbtntype.h uiphoneentry.h generictree.h screensaver.h

SOURCES += dialogbox.cpp lcddevice.cpp mythcontext.cpp mythwidgets.cpp 
SOURCES += oldsettings.cpp remotefile.cpp settings.cpp themedmenu.cpp
SOURCES += util.cpp mythwizard.cpp volumecontrol.h uitypes.cpp xmlparse.cpp
SOURCES += mythplugin.cpp mythdialogs.cpp audiooutput.cpp inetcomms.cpp 
SOURCES += httpcomms.cpp mythmedia.cpp uilistbtntype.cpp uiphoneentry.cpp
SOURCES += generictree.cpp

inc.path = $${PREFIX}/include/mythtv/
inc.files  = dialogbox.h lcddevice.h themedmenu.h mythcontext.h mythdbcon.h
inc.files += mythwidgets.h remotefile.h util.h oldsettings.h volumecontrol.h
inc.files += settings.h uitypes.h xmlparse.h mythplugin.h mythdialogs.h
inc.files += audiooutput.h inetcomms.h httpcomms.h mythmedia.h mythwizard.h
inc.files += uilistbtntype.h uiphoneentry.h generictree.h

!win32 {
    SOURCES += audiooutputoss.cpp mythcdrom.cpp mythmediamonitor.cpp
    HEADERS += audiooutputoss.h mythcdrom.h mythmediamonitor.h
    inc.files += mythcdrom.h mythmediamonitor.h
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

using_directx {
    HEADERS += audiooutputdx.h
    SOURCES += audiooutputdx.cpp
}

using_x11 {
    SOURCES += screensaver-x11.cpp
    LIBS += -L/usr/X11R6/lib -lXinerama
}

!using_x11 {
    SOURCES += screensaver-null.cpp
}
