include ( ../../settings.pro )

QT += network xml sql widgets
contains(QT_MAJOR_VERSION, 5): QT += script
android: QT += androidextras

TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += MYTH_API

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

POSTINC =

contains(INCLUDEPATH, /usr/include) {
  POSTINC += /usr/include
  INCLUDEPATH -= /usr/include
}
contains(INCLUDEPATH, /usr/local/include) {
  POSTINC += /usr/local/include
  INCLUDEPATH -= /usr/local/include
}
contains(INCLUDEPATH, /usr/X11R6/include) {
  POSTINC += /usr/X11R6/include
  INCLUDEPATH -= /usr/X11R6/include
}

# Input
HEADERS += audio/audiooutput.h audio/audiooutputbase.h audio/audiooutputnull.h
HEADERS += audio/audiooutpututil.h audio/audiooutputdownmix.h
HEADERS += audio/audioconvert.h
HEADERS += audio/audiooutputdigitalencoder.h audio/spdifencoder.h
HEADERS += audio/audiosettings.h audio/audiooutputsettings.h audio/pink.h
HEADERS += audio/volumebase.h audio/eldutils.h
HEADERS += audio/audiooutputgraph.h
HEADERS += backendselect.h dbsettings.h
HEADERS += langsettings.h
HEADERS +=
HEADERS += mythaverror.h mythcontext.h
HEADERS += mythexp.h mythmediamonitor.h
HEADERS += schemawizard.h
HEADERS += output.h
HEADERS +=
HEADERS += standardsettings.h
HEADERS += visual.h
HEADERS += storagegroupeditor.h
HEADERS += mythterminal.h
HEADERS += remoteutil.h
HEADERS += rawsettingseditor.h
HEADERS += programinfo.h          programinfoupdater.h
HEADERS += programtypes.h         recordingtypes.h
HEADERS += programtypeflags.h
HEADERS += rssparse.h
HEADERS += guistartup.h

SOURCES += audio/audiooutput.cpp audio/audiooutputbase.cpp
SOURCES += audio/spdifencoder.cpp audio/audiooutputdigitalencoder.cpp
SOURCES += audio/audiooutputnull.cpp
SOURCES += audio/audiooutpututil.cpp audio/audiooutputdownmix.cpp
SOURCES += audio/audioconvert.cpp
SOURCES += audio/audiosettings.cpp audio/audiooutputsettings.cpp audio/pink.cpp
SOURCES += audio/volumebase.cpp audio/eldutils.cpp
SOURCES += audio/audiooutputgraph.cpp
SOURCES += backendselect.cpp dbsettings.cpp
SOURCES += langsettings.cpp
SOURCES +=
SOURCES += mythaverror.cpp mythcontext.cpp
SOURCES += mythmediamonitor.cpp
SOURCES += schemawizard.cpp
SOURCES += output.cpp
SOURCES +=
SOURCES += standardsettings.cpp
SOURCES += storagegroupeditor.cpp
SOURCES += mythterminal.cpp
SOURCES += remoteutil.cpp
SOURCES += rawsettingseditor.cpp
SOURCES += programinfo.cpp        programinfoupdater.cpp
SOURCES += programtypes.cpp       recordingtypes.cpp
SOURCES += rssparse.cpp
SOURCES += guistartup.cpp

# This stuff is not Qt5 compatible..
# Really? It builds under Qt5, so lets let it
HEADERS += mythrssmanager.h             netutils.h
HEADERS += netgrabbermanager.h
SOURCES += mythrssmanager.cpp           netutils.cpp
SOURCES += netgrabbermanager.cpp

INCLUDEPATH += ../libmythfreesurround
INCLUDEPATH += ../libmythbase
INCLUDEPATH += ../.. ../ ./ ../libmythupnp ../libmythui
INCLUDEPATH += ../.. ../../external/FFmpeg
INCLUDEPATH += ../libmythservicecontracts
INCLUDEPATH += $${POSTINC}
DEPENDPATH += ../libmythfreesurround
DEPENDPATH += ../ ../libmythui ../libmythbase
DEPENDPATH += ../libmythupnp
DEPENDPATH += ./audio
DEPENDPATH += ../libmythservicecontracts

LIBS += -L../libmythbase           -lmythbase-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}
LIBS += -L../libmythfreesurround -lmythfreesurround-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../external/FFmpeg/libavutil  -lmythavutil
LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavformat  -lmythavformat
LIBS += -L../libmythservicecontracts         -lmythservicecontracts-$${LIBVERSION}
!using_libbluray_external {
    #INCLUDEPATH += ../../external/libmythbluray/src
    DEPENDPATH += ../../external/libmythbluray
    #LIBS += -L../../external/libmythbluray     -lmythbluray-$${LIBVERSION}
}

!win32-msvc* {
    !using_libbluray_external:POST_TARGETDEPS += ../../external/libmythbluray/libmythbluray-$${MYTH_LIB_EXT}
    POST_TARGETDEPS += ../../external/FFmpeg/libswresample/$$avLibName(swresample)
    POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
    POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
    POST_TARGETDEPS += ../libmythfreesurround/libmythfreesurround-$${MYTH_LIB_EXT}
}

# Install headers so that plugins can compile independently
inc.path = $${PREFIX}/include/mythtv/
inc.files  = dialogbox.h mythcontext.h
inc.files += mythwidgets.h remotefile.h volumecontrol.h
inc.files += audio/audiooutput.h audio/audiosettings.h
inc.files += audio/audiooutputsettings.h audio/audiooutpututil.h
inc.files += audio/audioconvert.h
inc.files += audio/volumebase.h audio/eldutils.h
inc.files += inetcomms.h schemawizard.h
inc.files += mythaverror.h mythmediamonitor.h
inc.files += visual.h output.h langsettings.h
inc.files += mythexp.h storagegroupeditor.h
inc.files += mythterminal.h       remoteutil.h
inc.files += programinfo.h
inc.files += programtypes.h       recordingtypes.h
inc.files += programtypeflags.h
inc.files += rssparse.h
inc.files += standardsettings.h

# This stuff is not Qt5 compatible..
# Really? It builds under Qt5, so lets let it
inc.files += mythrssmanager.h     netutils.h
inc.files += netgrabbermanager.h

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
    !android {
        using_qtdbus: QT += dbus
    }
}

android {
SOURCES += audio/audiooutputopensles.cpp
SOURCES += audio/audiooutputaudiotrack.cpp
HEADERS += audio/audiooutputopensles.h
HEADERS += audio/audiooutputaudiotrack.h
}

cygwin {
    QMAKE_LFLAGS_SHLIB += -Wl,--noinhibit-exec
    DEFINES += _WIN32
    #HEADERS += mediamonitor-windows.h
    #SOURCES += mediamonitor-windows.cpp
}

mingw:DEFINES += USING_MINGW

mingw | win32-msvc* {
    
    SOURCES += mediamonitor-windows.cpp audio/audiooutputwin.cpp
    SOURCES += audio/audiooutputdx.cpp
    HEADERS += mediamonitor-windows.h   audio/audiooutputwin.h
    HEADERS += audio/audiooutputdx.h
    LIBS += -lwinmm -lws2_32 -luser32 -lsamplerate -lSoundTouch
}

macx {
    HEADERS += audio/audiooutputca.h
    SOURCES += audio/audiooutputca.cpp

    darwin_da {
        SOURCES -= mediamonitor-unix.cpp
        HEADERS -= mediamonitor-unix.h
        HEADERS += mediamonitor-darwin.h
        SOURCES += mediamonitor-darwin.cpp
        DEFINES += USING_DARWIN_DA
    }

    # Mac OS X Frameworks
    darwin_da : LIBS += -framework DiskArbitration
    LIBS += -framework ApplicationServices
    LIBS += -framework AudioUnit
    LIBS += -framework AudioToolbox
    LIBS += -framework CoreAudio
    LIBS += -framework IOKit
}

INSTALLS += inc inc2

using_alsa {
    DEFINES += USING_ALSA
    HEADERS += audio/audiooutputalsa.h
    SOURCES += audio/audiooutputalsa.cpp
}

using_jack {
    DEFINES += USING_JACK
    HEADERS += audio/audiooutputjack.h
    SOURCES += audio/audiooutputjack.cpp
}

contains( HAVE_MMX, yes ) {
    HEADERS += ../../external/FFmpeg/libavutil/cpu.h
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

DISTFILES += \
    Makefile

test_clean.commands = -cd test/ && $(MAKE) -f Makefile clean
clean.depends = test_clean
QMAKE_EXTRA_TARGETS += test_clean clean
test_distclean.commands = -cd test/ && $(MAKE) -f Makefile distclean
distclean.depends = test_distclean
QMAKE_EXTRA_TARGETS += test_distclean distclean
