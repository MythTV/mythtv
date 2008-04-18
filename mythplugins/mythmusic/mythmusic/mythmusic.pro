include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include (config.pro)

!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}
 
INCLUDEPATH *= /usr/include/cdda
TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmusic
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

LIBS += -lmad -ltag -logg -lvorbisfile -lvorbis -lvorbisenc -lFLAC -lmp3lame

cdaudio: LIBS += -lcdaudio
paranoia:LIBS += -lcdda_paranoia -lcdda_interface

# Input
HEADERS += cddecoder.h cdrip.h constants.h databasebox.h
HEADERS += decoder.h flacdecoder.h flacencoder.h maddecoder.h mainvisual.h
HEADERS += metadata.h playbackbox.h playlist.h polygon.h 
HEADERS += streaminput.h synaesthesia.h encoder.h visualize.h avfdecoder.h
HEADERS += treecheckitem.h vorbisdecoder.h vorbisencoder.h polygon.h
HEADERS += bumpscope.h globalsettings.h lameencoder.h dbcheck.h
HEADERS += metaio.h metaiotaglib.h vcedit.h metaiooggvorbiscomment.h
HEADERS += metaioflacvorbiscomment.h metaioavfcomment.h
HEADERS += goom/filters.h goom/goomconfig.h goom/goom_core.h goom/graphic.h
HEADERS += goom/ifs.h goom/lines.h goom/mythgoom.h goom/drawmethods.h
HEADERS += goom/mmx.h goom/mathtools.h goom/tentacle3d.h goom/v3d.h
HEADERS += editmetadata.h smartplaylist.h search.h genres.h
HEADERS += treebuilders.h importmusic.h directoryfinder.h
HEADERS += filescanner.h libvisualplugin.h musicplayer.h miniplayer.h

SOURCES += cddecoder.cpp cdrip.cpp decoder.cpp 
SOURCES += flacdecoder.cpp flacencoder.cpp maddecoder.cpp main.cpp
SOURCES += mainvisual.cpp metadata.cpp playbackbox.cpp playlist.cpp 
SOURCES += streaminput.cpp encoder.cpp dbcheck.cpp
SOURCES += synaesthesia.cpp treecheckitem.cpp vorbisdecoder.cpp lameencoder.cpp
SOURCES += vorbisencoder.cpp visualize.cpp bumpscope.cpp globalsettings.cpp
SOURCES += databasebox.cpp genres.cpp
SOURCES += metaio.cpp metaiotaglib.cpp vcedit.c metaiooggvorbiscomment.cpp 
SOURCES += metaioflacvorbiscomment.cpp metaioavfcomment.cpp
SOURCES += goom/filters.c goom/goom_core.c goom/graphic.c goom/tentacle3d.c
SOURCES += goom/ifs.c goom/ifs_display.c goom/lines.c goom/surf3d.c 
SOURCES += goom/zoom_filter_mmx.c goom/zoom_filter_xmmx.c goom/mythgoom.cpp
SOURCES += avfdecoder.cpp editmetadata.cpp smartplaylist.cpp search.cpp
SOURCES += treebuilders.cpp importmusic.cpp directoryfinder.cpp
SOURCES += filescanner.cpp libvisualplugin.cpp musicplayer.cpp miniplayer.cpp

macx {
    SOURCES -= cddecoder.cpp
    SOURCES += cddecoder-darwin.cpp

    LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION -lmythavutil-$$LIBVERSION
    LIBS += -lmythavcodec-$$LIBVERSION -lmythavformat-$$LIBVERSION
    LIBS += -lmythupnp-$$LIBVERSION

    QT += network

    #QMAKE_LFLAGS += -flat_namespace -undefined suppress
    QMAKE_LFLAGS += -flat_namespace -undefined error
}

mingw {
    DEFINES += USING_MINGW

    HEADERS -= cdrip.h   importmusic.h
    SOURCES -= cdrip.cpp importmusic.cpp cddecoder.cpp
    SOURCES += cddecoder-windows.cpp

    LIBS += -logg
}
#The following line was inserted by qt3to4
QT += xml  sql opengl qt3support 
