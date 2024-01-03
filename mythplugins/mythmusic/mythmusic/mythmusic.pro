include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )
include (config.pro)

QT += xml sql opengl network widgets

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}
 
TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmusic
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${CONFIG_TAGLIB_INCLUDES}

LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -ltag -logg -lvorbisfile -lvorbis -lvorbisenc -lFLAC -lmp3lame
LIBS += -lmythmetadata-$$LIBVERSION
LIBS += -lmythtv-$$LIBVERSION

# Input
HEADERS += constants.h
HEADERS += decoder.h flacencoder.h mainvisual.h
HEADERS += playlist.h polygon.h
HEADERS += synaesthesia.h encoder.h visualize.h avfdecoder.h
HEADERS += vorbisencoder.h polygon.h
HEADERS += bumpscope.h lameencoder.h musicdbcheck.h
HEADERS += importmusic.h
HEADERS += mythgoom.h
HEADERS += editmetadata.h smartplaylist.h genres.h
HEADERS += musicplayer.h miniplayer.h
HEADERS += playlistcontainer.h musicdata.h
HEADERS += musiccommon.h decoderhandler.h pls.h
HEADERS += playlistview.h playlisteditorview.h 
HEADERS += visualizerview.h searchview.h streamview.h
HEADERS += generalsettings.h visualizationsettings.h
HEADERS += importsettings.h playersettings.h ratingsettings.h
HEADERS += remoteavformatcontext.h lyricsview.h

SOURCES += decoder.cpp
SOURCES += flacencoder.cpp mythmusic.cpp
SOURCES += mainvisual.cpp playlist.cpp
SOURCES += encoder.cpp musicdbcheck.cpp
SOURCES += synaesthesia.cpp lameencoder.cpp
SOURCES += vorbisencoder.cpp visualize.cpp bumpscope.cpp
SOURCES += genres.cpp importmusic.cpp
SOURCES += mythgoom.cpp
SOURCES += avfdecoder.cpp editmetadata.cpp smartplaylist.cpp
SOURCES += musicplayer.cpp miniplayer.cpp
SOURCES += playlistcontainer.cpp musicdata.cpp
SOURCES += musiccommon.cpp decoderhandler.cpp pls.cpp
SOURCES += playlistview.cpp playlisteditorview.cpp 
SOURCES += visualizerview.cpp searchview.cpp streamview.cpp
SOURCES += generalsettings.cpp visualizationsettings.cpp
SOURCES += importsettings.cpp playersettings.cpp ratingsettings.cpp
SOURCES += lyricsview.cpp

cdio {
    INCLUDEPATH -= $${SYSROOT}/usr/include/cdda
    INCLUDEPATH *= $${SYSROOT}/usr/include/cdio
    HEADERS += cddecoder.h cdrip.h
    SOURCES += cddecoder.cpp cdrip.cpp
    QT += network
    LIBS += -lcdio -lcdio_cdda -lcdio_paranoia
}

musicbrainz {
    HEADERS += musicbrainz.h
    SOURCES += musicbrainz.cpp
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

android {
    # to discriminate filters in a flat directory structure
    TARGET = mythplugin$${TARGET}
}

include ( ../../libs-targetfix.pro )
