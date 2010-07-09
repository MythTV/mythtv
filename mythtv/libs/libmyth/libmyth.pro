include ( ../../settings.pro )
 
TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += network xml sql

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += audiooutput.h audiooutputbase.h audiooutputnull.h
HEADERS += audiooutpututil.h audiooutputdownmix.h
HEADERS += audiooutputdigitalencoder.h audiosettings.h audiooutputsettings.h
HEADERS += backendselect.h dbsettings.h dialogbox.h
HEADERS += generictree.h langsettings.h
HEADERS += managedlist.h mythconfigdialogs.h mythconfiggroups.h
HEADERS += mythcontext.h mythdeque.h mythdialogs.h
HEADERS += mythevent.h mythexp.h mythmedia.h mythmediamonitor.h
HEADERS += mythplugin.h mythpluginapi.h 
HEADERS += mythwidgets.h mythwizard.h schemawizard.h
HEADERS += output.h 
HEADERS += settings.h 
HEADERS += uilistbtntype.h uitypes.h util.h mythuifilebrowser.h
HEADERS += volumebase.h visual.h xmlparse.h
HEADERS += mythhdd.h mythcdrom.h storagegroupeditor.h dbutil.h
HEADERS += mythcommandlineparser.h mythterminal.h
HEADERS += mythhttppool.h mythhttphandler.h
HEADERS += audiopulseutil.h       remoteutil.h
HEADERS += rawsettingseditor.h    autodeletedeque.h
HEADERS += programinfo.h          programinfoupdater.h
HEADERS += programtypes.h         recordingtypes.h
HEADERS += mythrssmanager.h       netgrabbermanager.h
HEADERS += rssparse.h             netutils.h

# remove when everything is switched to mythui
HEADERS += virtualkeyboard_qt.h

SOURCES += audiooutput.cpp audiooutputbase.cpp audiooutputnull.cpp
SOURCES += audiooutpututil.cpp audiooutputdownmix.cpp
SOURCES += audiooutputdigitalencoder.cpp audiosettings.cpp audiooutputsettings.cpp
SOURCES += backendselect.cpp dbsettings.cpp dialogbox.cpp
SOURCES += generictree.cpp langsettings.cpp
SOURCES += managedlist.cpp mythconfigdialogs.cpp mythconfiggroups.cpp
SOURCES += mythcontext.cpp mythdialogs.cpp
SOURCES += mythmedia.cpp mythmediamonitor.cpp
SOURCES += mythplugin.cpp 
SOURCES += mythwidgets.cpp mythwizard.cpp schemawizard.cpp
SOURCES += output.cpp 
SOURCES += settings.cpp
SOURCES += uilistbtntype.cpp uitypes.cpp util.cpp mythuifilebrowser.cpp
SOURCES += volumebase.cpp xmlparse.cpp
SOURCES += mythhdd.cpp mythcdrom.cpp storagegroupeditor.cpp dbutil.cpp
SOURCES += mythcommandlineparser.cpp mythterminal.cpp
SOURCES += mythhttppool.cpp mythhttphandler.cpp
SOURCES += audiopulseutil.cpp     remoteutil.cpp
SOURCES += rawsettingseditor.cpp
SOURCES += programinfo.cpp        programinfoupdater.cpp
SOURCES += programtypes.cpp       recordingtypes.cpp
SOURCES += mythrssmanager.cpp     netgrabbermanager.cpp
SOURCES += rssparse.cpp           netutils.cpp

# remove when everything is switched to mythui
SOURCES += virtualkeyboard_qt.cpp


INCLUDEPATH += ../libmythsamplerate ../libmythsoundtouch ../libmythfreesurround
INCLUDEPATH += ../libavcodec ../libavutil ../libmythdb
INCLUDEPATH += ../.. ../ ./ ../libmythupnp ../libmythui
DEPENDPATH += ../libmythsamplerate ../libmythsoundtouch
DEPENDPATH += ../libmythfreesurround
DEPENDPATH += ../libavcodec ../libavutil
DEPENDPATH += ../ ../libmythui ../libmythdb
DEPENDPATH += ../libmythupnp


LIBS += -L../libmythsamplerate   -lmythsamplerate-$${LIBVERSION}
LIBS += -L../libmythsoundtouch   -lmythsoundtouch-$${LIBVERSION}
LIBS += -L../libmythdb           -lmythdb-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}
LIBS += -L../libmythfreesurround -lmythfreesurround-$${LIBVERSION}
LIBS += -L../libavcodec          -lmythavcodec-$${LIBVERSION}
LIBS += -L../libavutil           -lmythavutil-$${LIBVERSION}
unix:LIBS += -ldl

TARGETDEPS += ../libmythsamplerate/libmythsamplerate-$${MYTH_LIB_EXT}
TARGETDEPS += ../libmythsoundtouch/libmythsoundtouch-$${MYTH_LIB_EXT}
TARGETDEPS += ../libmythfreesurround/libmythfreesurround-$${MYTH_LIB_EXT}

# Install headers so that plugins can compile independently
inc.path = $${PREFIX}/include/mythtv/
inc.files  = dialogbox.h mythcontext.h
inc.files += mythwidgets.h remotefile.h oldsettings.h volumecontrol.h
inc.files += settings.h uitypes.h xmlparse.h mythplugin.h mythdialogs.h
inc.files += audiooutput.h audiosettings.h audiooutputsettings.h util.h
inc.files += inetcomms.h mythmedia.h mythcdrom.h mythwizard.h schemawizard.h dbutil.h
inc.files += uilistbtntype.h generictree.h managedlist.h mythmediamonitor.h
inc.files += visual.h volumebase.h output.h langsettings.h
inc.files += mythexp.h mythpluginapi.h storagegroupeditor.h
inc.files += mythconfigdialogs.h mythconfiggroups.h
inc.files += mythterminal.h mythdeque.h mythuifilebrowser.h
inc.files += mythhttppool.h       remoteutil.h
inc.files += programinfo.h        autodeletedeque.h
inc.files += programtypes.h       recordingtypes.h
inc.files += mythrssmanager.h     netgrabbermanager.h
inc.files += rssparse.h           netutils.h

# remove when everything is switched to mythui
inc.files += virtualkeyboard_qt.h

# Allow both #include <blah.h> and #include <libmyth/blah.h>
inc2.path  = $${PREFIX}/include/mythtv/libmyth
inc2.files = $${inc.files}

using_oss {
    DEFINES += USING_OSS
    SOURCES += audiooutputoss.cpp
    HEADERS += audiooutputoss.h
    LIBS += $$OSS_LIBS
}

using_pulse {
    DEFINES += USING_PULSE
    LIBS += $$PULSE_LIBS
    using_pulseoutput {
        DEFINES += USING_PULSEOUTPUT
        HEADERS += audiooutputpulse.h
        SOURCES += audiooutputpulse.cpp
    }
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
    DEFINES += USING_MINGW
    SOURCES += mediamonitor-windows.cpp audiooutputwin.cpp audiooutputdx.cpp
    HEADERS += mediamonitor-windows.h   audiooutputwin.h   audiooutputdx.h
    LIBS += -lpthread -lwinmm -lws2_32
}

macx {
    HEADERS += audiooutputca.h
    SOURCES += audiooutputca.cpp
    HEADERS += mythcdrom-darwin.h
    SOURCES += mythcdrom-darwin.cpp

    darwin_da {
        SOURCES -= mediamonitor-unix.cpp
        HEADERS -= mediamonitor-unix.h
        HEADERS += mediamonitor-darwin.h
        SOURCES += mediamonitor-darwin.cpp
        DEFINES += USING_DARWIN_DA
    }

    # Mac OS X Frameworks
    FWKS = ApplicationServices AudioUnit Carbon CoreAudio IOKit

    darwin_da : FWKS += DiskArbitration

    # The following trick is tidier, and shortens the command line, but it
    # depends on the shell expanding Csh-style braces. Luckily, Bash & Zsh do.
    FC = $$join(FWKS,",","{","}")

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")
}

linux {
    SOURCES += mythcdrom-linux.cpp
    HEADERS += mythcdrom-linux.h
}

freebsd {
    SOURCES += mythcdrom-freebsd.cpp
    HEADERS += mythcdrom-freebsd.h
    LIBS    -= -ldl
}

INSTALLS += inc inc2

using_alsa {
    DEFINES += USE_ALSA
    HEADERS += audiooutputalsa.h
    SOURCES += audiooutputalsa.cpp
    LIBS += $$ALSA_LIBS
}

using_jack {
    DEFINES += USE_JACK
    HEADERS += audiooutputjack.h
    SOURCES += audiooutputjack.cpp
    LIBS += $$JACK_LIBS
}

contains( HAVE_MMX, yes ) {
    HEADERS += ../../libs/libavcodec/x86/mmx.h ../../libs/libavcodec/dsputil.h
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

using_libudev : LIBS += $${CONFIG_LIBUDEV_LIBS}


include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
