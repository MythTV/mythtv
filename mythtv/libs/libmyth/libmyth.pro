include ( ../../config.mak )
include ( ../../settings.pro )
 
TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += network xml opengl sql qt3support
qt3support:DEFINES += QT3SUPPORT

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += audiooutput.h audiooutputbase.h audiooutputnull.h
HEADERS += audiooutputdigitalencoder.h audiosettings.cpp
HEADERS += backendselect.h dbsettings.h dialogbox.h
HEADERS += generictree.h langsettings.h
HEADERS += managedlist.h mythconfigdialogs.h mythconfiggroups.h
HEADERS += mythcontext.h mythdeque.h mythdialogs.h
HEADERS += mythevent.h mythexp.h mythmedia.h mythmediamonitor.h
HEADERS += mythplugin.h mythpluginapi.h 
HEADERS += mythwidgets.h mythwizard.h schemawizard.h
HEADERS += output.h remotefile.h
HEADERS += settings.h 
HEADERS += uilistbtntype.h uitypes.h util.h
HEADERS += volumebase.h virtualkeyboard.h visual.h xmlparse.h
HEADERS += mythhdd.h mythcdrom.h storagegroup.h dbutil.h
HEADERS += mythcommandlineparser.h mythterminal.h

SOURCES += audiooutput.cpp audiooutputbase.cpp audiooutputnull.cpp
SOURCES += audiooutputdigitalencoder.cpp audiosettings.cpp
SOURCES += backendselect.cpp dbsettings.cpp dialogbox.cpp
SOURCES += generictree.cpp langsettings.cpp
SOURCES += managedlist.cpp mythconfigdialogs.cpp mythconfiggroups.cpp
SOURCES += mythcontext.cpp mythdialogs.cpp
SOURCES += mythmedia.cpp mythmediamonitor.cpp
SOURCES += mythplugin.cpp 
SOURCES += mythwidgets.cpp mythwizard.cpp schemawizard.cpp
SOURCES += output.cpp remotefile.cpp
SOURCES += settings.cpp
SOURCES += uilistbtntype.cpp uitypes.cpp util.cpp
SOURCES += volumebase.cpp virtualkeyboard.cpp xmlparse.cpp
SOURCES += mythhdd.cpp mythcdrom.cpp storagegroup.cpp dbutil.cpp
SOURCES += mythcommandlineparser.cpp mythterminal.cpp

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
inc.files += audiooutput.h audiosettings.h util.h
inc.files += inetcomms.h mythmedia.h mythwizard.h schemawizard.h dbutil.h
inc.files += uilistbtntype.h generictree.h managedlist.h mythmediamonitor.h
inc.files += visual.h volumebase.h output.h langsettings.h
inc.files += virtualkeyboard.h
inc.files += mythexp.h mythpluginapi.h storagegroup.h
inc.files += mythconfigdialogs.h mythconfiggroups.h
inc.files += mythterminal.h

# Allow both #include <blah.h> and #include <libmyth/blah.h>
inc2.path  = $${PREFIX}/include/mythtv/libmyth
inc2.files = $${inc.files}

using_oss {
    DEFINES += USING_OSS
    SOURCES += audiooutputoss.cpp
    HEADERS += audiooutputoss.h
    LIBS += $$OSS_LIBS
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

    LIBS += -lpthread -lwinmm -lws2_32
}

macx {
    HEADERS += audiooutputca.h
    SOURCES += audiooutputca.cpp

    darwin_da {
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
