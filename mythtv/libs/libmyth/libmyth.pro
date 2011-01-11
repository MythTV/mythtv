include ( ../../settings.pro )
 
TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += network xml sql

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += audio/audiooutput.h audio/audiooutputbase.h audio/audiooutputnull.h
HEADERS += audio/audiooutpututil.h audio/audiooutputdownmix.h
HEADERS += audio/audiooutputdigitalencoder.h audio/spdifencoder.h
HEADERS += audio/audiosettings.h audio/audiooutputsettings.h audio/pink.h
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
HEADERS += remoteutil.h
HEADERS += rawsettingseditor.h    autodeletedeque.h
HEADERS += programinfo.h          programinfoupdater.h
HEADERS += programtypes.h         recordingtypes.h
HEADERS += mythrssmanager.h       netgrabbermanager.h
HEADERS += rssparse.h             netutils.h

# remove when everything is switched to mythui
HEADERS += virtualkeyboard_qt.h

SOURCES += audio/audiooutput.cpp audio/audiooutputbase.cpp
SOURCES += audio/spdifencoder.cpp audio/audiooutputdigitalencoder.cpp
SOURCES += audio/audiooutputnull.cpp
SOURCES += audio/audiooutpututil.cpp audio/audiooutputdownmix.cpp
SOURCES += audio/audiosettings.cpp audio/audiooutputsettings.cpp audio/pink.c
SOURCES += volumebase.cpp xmlparse.cpp

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
SOURCES += mythhdd.cpp mythcdrom.cpp storagegroupeditor.cpp dbutil.cpp
SOURCES += mythcommandlineparser.cpp mythterminal.cpp
SOURCES += mythhttppool.cpp mythhttphandler.cpp
SOURCES += remoteutil.cpp
SOURCES += rawsettingseditor.cpp
SOURCES += programinfo.cpp        programinfoupdater.cpp
SOURCES += programtypes.cpp       recordingtypes.cpp
SOURCES += mythrssmanager.cpp     netgrabbermanager.cpp
SOURCES += rssparse.cpp           netutils.cpp

# remove when everything is switched to mythui
SOURCES += virtualkeyboard_qt.cpp


INCLUDEPATH += ../libmythsamplerate ../libmythsoundtouch ../libmythfreesurround
INCLUDEPATH += ../libmythdb
INCLUDEPATH += ../.. ../ ./ ../libmythupnp ../libmythui
INCLUDEPATH += ../../external/FFmpeg
DEPENDPATH += ../libmythsamplerate ../libmythsoundtouch
DEPENDPATH += ../libmythfreesurround
DEPENDPATH += ../ ../libmythui ../libmythdb
DEPENDPATH += ../libmythupnp
DEPENDPATH += ./audio

LIBS += -L../libmythsamplerate   -lmythsamplerate-$${LIBVERSION}
LIBS += -L../libmythsoundtouch   -lmythsoundtouch-$${LIBVERSION}
LIBS += -L../libmythdb           -lmythdb-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}
LIBS += -L../libmythfreesurround -lmythfreesurround-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavcore  -lmythavcodec
LIBS += -L../../external/FFmpeg/libavutil  -lmythavutil
LIBS += -L../../external/FFmpeg/libavformat  -lmythavformat

TARGETDEPS += ../libmythsamplerate/libmythsamplerate-$${MYTH_LIB_EXT}
TARGETDEPS += ../libmythsoundtouch/libmythsoundtouch-$${MYTH_LIB_EXT}
TARGETDEPS += ../libmythfreesurround/libmythfreesurround-$${MYTH_LIB_EXT}
TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
TARGETDEPS += ../../external/FFmpeg/libavcore/$$avLibName(avcore)
TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)

# Install headers so that plugins can compile independently
inc.path = $${PREFIX}/include/mythtv/
inc.files  = dialogbox.h mythcontext.h
inc.files += mythwidgets.h remotefile.h oldsettings.h volumecontrol.h
inc.files += settings.h uitypes.h xmlparse.h mythplugin.h mythdialogs.h
inc.files += audio/audiooutput.h audio/audiosettings.h
inc.files += audio/audiooutputsettings.h
inc.files += util.h dbutil.h
inc.files += inetcomms.h mythmedia.h mythcdrom.h mythwizard.h schemawizard.h
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
    SOURCES += audio/audiooutputoss.cpp
    HEADERS += audio/audiooutputoss.h
}

using_pulse {
    DEFINES += USING_PULSE
    HEADERS += audio/audiopulsehandler.h
    SOURCES += audio/audiopulsehandler.cpp
    using_pulseoutput {
        DEFINES += USING_PULSEOUTPUT
        HEADERS += audio/audiooutputpulse.h
        SOURCES += audio/audiooutputpulse.cpp
    }
}

unix:!cygwin {
    SOURCES += mediamonitor-unix.cpp
    HEADERS += mediamonitor-unix.h
}

linux:DEFINES += linux

cygwin {
    QMAKE_LFLAGS_SHLIB += -Wl,--noinhibit-exec
    DEFINES += _WIN32
    #HEADERS += mediamonitor-windows.h
    #SOURCES += mediamonitor-windows.cpp
}

mingw {
    DEFINES += USING_MINGW
    SOURCES += mediamonitor-windows.cpp audio/audiooutputwin.cpp
    SOURCES += audio/audiooutputdx.cpp
    HEADERS += mediamonitor-windows.h   audio/audiooutputwin.h
    HEADERS += audio/audiooutputdx.h
    LIBS += -lpthread -lwinmm -lws2_32
}

macx {
    HEADERS += audio/audiooutputca.h
    SOURCES += audio/audiooutputca.cpp
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
}

INSTALLS += inc inc2

using_alsa {
    DEFINES += USE_ALSA
    HEADERS += audio/audiooutputalsa.h
    SOURCES += audio/audiooutputalsa.cpp
}

using_jack {
    DEFINES += USE_JACK
    HEADERS += audio/audiooutputjack.h
    SOURCES += audio/audiooutputjack.cpp
}

contains( HAVE_MMX, yes ) {
    HEADERS += ../../external/FFmpeg/libavcodec/x86/mmx.h ../../external/FFmpeg/libavcodec/dsputil.h
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
