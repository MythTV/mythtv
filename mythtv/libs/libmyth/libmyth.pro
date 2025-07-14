include ( ../../settings.pro )

QT += network xml sql widgets
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
HEADERS += audio/freesurround.h
HEADERS += audio/freesurround_decoder.h
HEADERS += backendselect.h
HEADERS += mythaverror.h
HEADERS += mythavframe.h
HEADERS += mythcontext.h
HEADERS += mythexp.h
HEADERS += output.h
HEADERS += visual.h

SOURCES += audio/audiooutput.cpp audio/audiooutputbase.cpp
SOURCES += audio/spdifencoder.cpp audio/audiooutputdigitalencoder.cpp
SOURCES += audio/audiooutputnull.cpp
SOURCES += audio/audiooutpututil.cpp audio/audiooutputdownmix.cpp
SOURCES += audio/audioconvert.cpp
SOURCES += audio/audiosettings.cpp audio/audiooutputsettings.cpp audio/pink.cpp
SOURCES += audio/volumebase.cpp audio/eldutils.cpp
SOURCES += audio/audiooutputgraph.cpp
SOURCES += audio/freesurround.cpp
SOURCES += audio/freesurround_decoder.cpp
SOURCES += backendselect.cpp
SOURCES += mythaverror.cpp mythcontext.cpp
SOURCES += output.cpp

INCLUDEPATH += ..
INCLUDEPATH += ../../external/FFmpeg
INCLUDEPATH += $${POSTINC}

LIBS += -L../libmythbase           -lmythbase-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}
LIBS += -L../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../external/FFmpeg/libavutil  -lmythavutil
LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavformat  -lmythavformat
!using_system_libbluray {
    #INCLUDEPATH += ../../external/libmythbluray/src
    DEPENDPATH += ../../external/libmythbluray
    #LIBS += -L../../external/libmythbluray     -lmythbluray-$${LIBVERSION}
}

!win32-msvc* {
    !using_system_libbluray:POST_TARGETDEPS += ../../external/libmythbluray/libmythbluray-$${MYTH_LIB_EXT}
    POST_TARGETDEPS += ../../external/FFmpeg/libswresample/$$avLibName(swresample)
    POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
    POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
}

# Install headers so that plugins can compile independently
inc.path = $${PREFIX}/include/mythtv/libmyth
inc.files  = dialogbox.h mythcontext.h
inc.files += mythwidgets.h remotefile.h volumecontrol.h
inc.files += inetcomms.h
inc.files += mythaverror.h
inc.files += mythavframe.h
inc.files += visual.h output.h
inc.files += mythexp.h

inc2.path = $${PREFIX}/include/mythtv/libmyth/audio
inc2.files += audio/audiooutput.h audio/audiosettings.h
inc2.files += audio/audiooutputsettings.h audio/audiooutpututil.h
inc2.files += audio/audioconvert.h
inc2.files += audio/volumebase.h audio/eldutils.h

using_oss {
    SOURCES += audio/audiooutputoss.cpp
    HEADERS += audio/audiooutputoss.h
}

using_pulse {
    HEADERS += audio/audiopulsehandler.h
    SOURCES += audio/audiopulsehandler.cpp
    using_pulseoutput {
        HEADERS += audio/audiooutputpulse.h
        SOURCES += audio/audiooutputpulse.cpp
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
}

mingw | win32-msvc* {
    
    SOURCES += audio/audiooutputwin.cpp
    SOURCES += audio/audiooutputdx.cpp
    HEADERS += audio/audiooutputwin.h
    HEADERS += audio/audiooutputdx.h
    LIBS += -lwinmm -lws2_32 -luser32 -lsamplerate -lSoundTouch
}

macx {
    HEADERS += audio/audiooutputca.h
    SOURCES += audio/audiooutputca.cpp

    # Mac OS X Frameworks
    LIBS += -framework ApplicationServices
    LIBS += -framework AudioUnit
    LIBS += -framework AudioToolbox
    LIBS += -framework CoreAudio
}

INSTALLS += inc inc2

using_alsa {
    HEADERS += audio/audiooutputalsa.h
    SOURCES += audio/audiooutputalsa.cpp
}

using_jack {
    HEADERS += audio/audiooutputjack.h
    SOURCES += audio/audiooutputjack.cpp
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
