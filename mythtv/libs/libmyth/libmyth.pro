include ( ../../settings.pro )

TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.13.0

# Input
HEADERS += dialogbox.h lcddevice.h mythcontext.h mythwidgets.h oldsettings.h  
HEADERS += remotefile.h settings.h themedmenu.h util.h mythwizard.h
HEADERS += volumecontrol.h uitypes.h xmlparse.h mythplugin.h
HEADERS += mythdialogs.h audiooutput.h audiooutputoss.h inetcomms.h
HEADERS += httpcomms.h mythmedia.h mythcdrom.h mythmediamonitor.h

SOURCES += dialogbox.cpp lcddevice.cpp mythcontext.cpp mythwidgets.cpp 
SOURCES += oldsettings.cpp remotefile.cpp settings.cpp themedmenu.cpp
SOURCES += util.cpp mythwizard.cpp volumecontrol.h uitypes.cpp xmlparse.cpp
SOURCES += mythplugin.cpp mythdialogs.cpp audiooutput.cpp audiooutputoss.cpp
SOURCES += inetcomms.cpp httpcomms.cpp mythmedia.cpp mythcdrom.cpp 
SOURCES += mythmediamonitor.cpp

inc.path = $${PREFIX}/include/mythtv/
inc.files  = dialogbox.h lcddevice.h themedmenu.h mythcontext.h 
inc.files += mythwidgets.h remotefile.h util.h oldsettings.h volumecontrol.h
inc.files += settings.h uitypes.h xmlparse.h mythplugin.h mythdialogs.h
inc.files += audiooutput.h inetcomms.h httpcomms.h mythmedia.h mythcdrom.h 
inc.files += mythmediamonitor.h mythwizard.h

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

INSTALLS += inc
