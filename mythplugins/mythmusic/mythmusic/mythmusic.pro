include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )
include (config.pro)

QT += xml sql opengl network webkit

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}
 
INCLUDEPATH *= $${SYSROOT}/usr/include/cdda
TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmusic
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -ltag -logg -lvorbisfile -lvorbis -lvorbisenc -lFLAC -lmp3lame

cdaudio: LIBS += -lcdaudio
paranoia:LIBS += -lcdda_paranoia -lcdda_interface
 
# Input
HEADERS += cddecoder.h cdrip.h constants.h
HEADERS += decoder.h flacencoder.h mainvisual.h
HEADERS += metadata.h playlist.h polygon.h
HEADERS += streaminput.h synaesthesia.h encoder.h visualize.h avfdecoder.h
HEADERS += vorbisencoder.h polygon.h
HEADERS += bumpscope.h globalsettings.h lameencoder.h dbcheck.h
HEADERS += metaio.h metaiotaglib.h
HEADERS += metaioflacvorbis.h metaioavfcomment.h metaiomp4.h
HEADERS += metaiowavpack.h metaioid3.h metaiooggvorbis.h
HEADERS += goom/filters.h goom/goomconfig.h goom/goom_core.h goom/graphic.h
HEADERS += goom/ifs.h goom/lines.h goom/mythgoom.h goom/drawmethods.h
HEADERS += goom/mmx.h goom/mathtools.h goom/tentacle3d.h goom/v3d.h
HEADERS += editmetadata.h smartplaylist.h genres.h
HEADERS += importmusic.h
HEADERS += filescanner.h musicplayer.h miniplayer.h
HEADERS += playlistcontainer.h
HEADERS += musiccommon.h decoderhandler.h pls.h shoutcast.h
HEADERS += playlistview.h playlisteditorview.h 
HEADERS += visualizerview.h searchview.h musicutils.h

SOURCES += cddecoder.cpp cdrip.cpp decoder.cpp
SOURCES += flacencoder.cpp main.cpp
SOURCES += mainvisual.cpp metadata.cpp playlist.cpp
SOURCES += streaminput.cpp encoder.cpp dbcheck.cpp
SOURCES += synaesthesia.cpp lameencoder.cpp
SOURCES += vorbisencoder.cpp visualize.cpp bumpscope.cpp globalsettings.cpp
SOURCES += genres.cpp
SOURCES += metaio.cpp metaiotaglib.cpp
SOURCES += metaioflacvorbis.cpp metaioavfcomment.cpp metaiomp4.cpp
SOURCES += metaiowavpack.cpp metaioid3.cpp metaiooggvorbis.cpp
SOURCES += goom/filters.c goom/goom_core.c goom/graphic.c goom/tentacle3d.c
SOURCES += goom/ifs.c goom/ifs_display.c goom/lines.c goom/surf3d.c
SOURCES += goom/zoom_filter_mmx.c goom/zoom_filter_xmmx.c goom/mythgoom.cpp
SOURCES += avfdecoder.cpp editmetadata.cpp smartplaylist.cpp
SOURCES += importmusic.cpp
SOURCES += filescanner.cpp musicplayer.cpp miniplayer.cpp
SOURCES += playlistcontainer.cpp
SOURCES += musiccommon.cpp decoderhandler.cpp pls.cpp shoutcast.cpp
SOURCES += playlistview.cpp playlisteditorview.cpp 
SOURCES += visualizerview.cpp searchview.cpp musicutils.cpp

macx {
    SOURCES -= cddecoder.cpp
    SOURCES += cddecoder-darwin.cpp

    QT += network
}

mingw {
    HEADERS -= cdrip.h   importmusic.h
    SOURCES -= cdrip.cpp importmusic.cpp cddecoder.cpp
    SOURCES += cddecoder-windows.cpp

    LIBS += -logg

    # libcdaudio needs ...
    LIBS += -lwsock32
}

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
    QMAKE_CXXFLAGS_SHLIB += -fvisibility=hidden
}

include ( ../../libs-targetfix.pro )
