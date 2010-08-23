include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )
include (config.pro)

QT += xml sql opengl qt3support network

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
LIBS += -lmythavcore
LIBS += -lmythavutil
LIBS += -ltag -logg -lvorbisfile -lvorbis -lvorbisenc -lFLAC -lmp3lame

cdaudio: LIBS += -lcdaudio
paranoia:LIBS += -lcdda_paranoia -lcdda_interface

# Input
HEADERS += cddecoder.h cdrip.h constants.h databasebox.h
HEADERS += decoder.h flacencoder.h mainvisual.h
HEADERS += metadata.h playbackbox.h playlist.h polygon.h
HEADERS += streaminput.h synaesthesia.h encoder.h visualize.h avfdecoder.h
HEADERS += treecheckitem.h vorbisencoder.h polygon.h
HEADERS += bumpscope.h globalsettings.h lameencoder.h dbcheck.h
HEADERS += metaio.h metaiotaglib.h
HEADERS += metaioflacvorbis.h metaioavfcomment.h metaiomp4.h
HEADERS += metaiowavpack.h metaioid3.h metaiooggvorbis.h
HEADERS += goom/filters.h goom/goomconfig.h goom/goom_core.h goom/graphic.h
HEADERS += goom/ifs.h goom/lines.h goom/mythgoom.h goom/drawmethods.h
HEADERS += goom/mmx.h goom/mathtools.h goom/tentacle3d.h goom/v3d.h
HEADERS += editmetadata.h smartplaylist.h search.h genres.h
HEADERS += treebuilders.h importmusic.h
HEADERS += filescanner.h libvisualplugin.h musicplayer.h miniplayer.h
HEADERS += playlistcontainer.h
HEADERS += mythlistview-qt3.h mythlistbox-qt3.h
HEADERS += musiccommon.h decoderhandler.h pls.h shoutcast.h

SOURCES += cddecoder.cpp cdrip.cpp decoder.cpp
SOURCES += flacencoder.cpp main.cpp
SOURCES += mainvisual.cpp metadata.cpp playbackbox.cpp playlist.cpp
SOURCES += streaminput.cpp encoder.cpp dbcheck.cpp
SOURCES += synaesthesia.cpp treecheckitem.cpp lameencoder.cpp
SOURCES += vorbisencoder.cpp visualize.cpp bumpscope.cpp globalsettings.cpp
SOURCES += databasebox.cpp genres.cpp
SOURCES += metaio.cpp metaiotaglib.cpp
SOURCES += metaioflacvorbis.cpp metaioavfcomment.cpp metaiomp4.cpp
SOURCES += metaiowavpack.cpp metaioid3.cpp metaiooggvorbis.cpp
SOURCES += goom/filters.c goom/goom_core.c goom/graphic.c goom/tentacle3d.c
SOURCES += goom/ifs.c goom/ifs_display.c goom/lines.c goom/surf3d.c
SOURCES += goom/zoom_filter_mmx.c goom/zoom_filter_xmmx.c goom/mythgoom.cpp
SOURCES += avfdecoder.cpp editmetadata.cpp smartplaylist.cpp search.cpp
SOURCES += treebuilders.cpp importmusic.cpp
SOURCES += filescanner.cpp libvisualplugin.cpp musicplayer.cpp miniplayer.cpp
SOURCES += playlistcontainer.cpp
SOURCES += mythlistview-qt3.cpp mythlistbox-qt3.cpp
SOURCES += musiccommon.cpp decoderhandler.cpp pls.cpp shoutcast.cpp

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

include ( ../../libs-targetfix.pro )
