include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )
include (config.pro)

QT += xml sql opengl network webkit

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}
 
TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmusic
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${SYSROOT}$${PREFIX}/include/mythtv/metadata
INCLUDEPATH += $${CONFIG_TAGLIB_INCLUDES}

LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -ltag -logg -lvorbisfile -lvorbis -lvorbisenc -lFLAC -lmp3lame
LIBS += -lmythmetadata-$$LIBVERSION

# Input
HEADERS += constants.h
HEADERS += decoder.h flacencoder.h mainvisual.h
HEADERS += playlist.h polygon.h
HEADERS += synaesthesia.h encoder.h visualize.h avfdecoder.h
HEADERS += vorbisencoder.h polygon.h
HEADERS += bumpscope.h lameencoder.h dbcheck.h
HEADERS += importmusic.h
HEADERS += goom/filters.h goom/goomconfig.h goom/goom_core.h goom/graphic.h
HEADERS += goom/ifs.h goom/lines.h goom/mythgoom.h goom/drawmethods.h
HEADERS += goom/mmx.h goom/mathtools.h goom/tentacle3d.h goom/v3d.h
HEADERS += editmetadata.h smartplaylist.h genres.h
HEADERS += filescanner.h musicplayer.h miniplayer.h
HEADERS += playlistcontainer.h musicdata.h
HEADERS += musiccommon.h decoderhandler.h pls.h shoutcast.h
HEADERS += playlistview.h playlisteditorview.h 
HEADERS += visualizerview.h searchview.h streamview.h
HEADERS += generalsettings.h visualizationsettings.h
HEADERS += importsettings.h playersettings.h ratingsettings.h

SOURCES += decoder.cpp
SOURCES += flacencoder.cpp main.cpp
SOURCES += mainvisual.cpp playlist.cpp
SOURCES += encoder.cpp dbcheck.cpp
SOURCES += synaesthesia.cpp lameencoder.cpp
SOURCES += vorbisencoder.cpp visualize.cpp bumpscope.cpp
SOURCES += genres.cpp importmusic.cpp
SOURCES += goom/filters.c goom/goom_core.c goom/graphic.c goom/tentacle3d.c
SOURCES += goom/ifs.c goom/ifs_display.c goom/lines.c goom/surf3d.c
SOURCES += goom/zoom_filter_mmx.c goom/zoom_filter_xmmx.c goom/mythgoom.cpp
SOURCES += avfdecoder.cpp editmetadata.cpp smartplaylist.cpp
SOURCES += filescanner.cpp musicplayer.cpp miniplayer.cpp
SOURCES += playlistcontainer.cpp musicdata.cpp
SOURCES += musiccommon.cpp decoderhandler.cpp pls.cpp shoutcast.cpp
SOURCES += playlistview.cpp playlisteditorview.cpp 
SOURCES += visualizerview.cpp searchview.cpp streamview.cpp
SOURCES += generalsettings.cpp visualizationsettings.cpp
SOURCES += importsettings.cpp playersettings.cpp ratingsettings.cpp

cdio {
    INCLUDEPATH -= $${SYSROOT}/usr/include/cdda
    INCLUDEPATH *= $${SYSROOT}/usr/include/cdio
    HEADERS += cddecoder.h cdrip.h cddb.h
    SOURCES += cddecoder.cpp cdrip.cpp cddb.cpp
    QT += network
    LIBS += -lcdio -lcdio_cdda -lcdio_paranoia
}

mingw {
    LIBS += -logg

    # flac needs ...
    LIBS += -lwsock32
}

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
    QMAKE_CXXFLAGS_SHLIB += -fvisibility=hidden
}

include ( ../../libs-targetfix.pro )
