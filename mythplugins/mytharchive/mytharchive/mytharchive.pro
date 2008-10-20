include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )
include (config.pro)
 
!exists( config.pro ) {
   error(Missing config.pro: please run the configure script)
}

INCLUDEPATH += $${PREFIX}/include/mythtv $${PREFIX}/include/mythtv/ffmpeg 
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythtv

LIBS += -lmythtv-$$LIBVERSION -lmythavformat-$$LIBVERSION
using_live: LIBS += -lmythlivemedia-$$LIBVERSION
using_mheg: LIBS += -lmythfreemheg-$$LIBVERSION

TEMPLATE = lib opengl
CONFIG += plugin thread
TARGET = mytharchive
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

###############################################
# shared files
###############################################
HEADERS += archivesettings.h logviewer.h fileselector.h
HEADERS += recordingselector.h videoselector.h dbcheck.h
HEADERS += archiveutil.h

SOURCES += main.cpp archivesettings.cpp logviewer.cpp 
SOURCES += fileselector.cpp recordingselector.cpp videoselector.cpp
SOURCES += dbcheck.cpp archiveutil.cpp

#######################################
# dvd creation support
#######################################
createdvd {
    burnuifiles.path = $${PREFIX}/share/mythtv/themes/default
    burnuifiles.files = mythburn-ui.xml

    scriptfiles.path = $${PREFIX}/share/mythtv/mytharchive/scripts
    scriptfiles.files = ../mythburn/scripts/*

    introfiles.path = $${PREFIX}/share/mythtv/mytharchive/intro
    introfiles.files = ../mythburn/intro/*.mpg

    musicfiles.path = $${PREFIX}/share/mythtv/mytharchive/music
    musicfiles.files = ../mythburn/music/*.ac3

    imagefiles.path = $${PREFIX}/share/mythtv/mytharchive/images
    imagefiles.files = ../mythburn/images/*.png

    themefiles.path = $${PREFIX}/share/mythtv/mytharchive/themes
    themefiles.files = ../mythburn/themes/*

    profilefiles.path = $${PREFIX}/share/mythtv/mytharchive/encoder_profiles
    profilefiles.files = ../mythburn/encoder_profiles/*.xml

    #fontfiles.path = $${PREFIX}/share/mythtv
    #fontfiles.files = ../fonts/*.ttf

    INSTALLS +=  scriptfiles introfiles themefiles imagefiles musicfiles 
    INSTALLS +=  profilefiles burnuifiles

    HEADERS += mythburnwizard.h editmetadata.h thumbfinder.h

    SOURCES += mythburnwizard.cpp editmetadata.cpp thumbfinder.cpp
}

#######################################
# native archive creation support
#######################################
createnative {
    nativeuifiles.path = $${PREFIX}/share/mythtv/themes/default
    nativeuifiles.files = mythnative-ui.xml 

    INSTALLS += nativeuifiles 

    HEADERS += exportnativewizard.h importnativewizard.h

    SOURCES += exportnativewizard.cpp importnativewizard.cpp
}

#######################################
# reencode video support
#######################################
encodevideo {

}


#The following line was inserted by qt3to4
QT += xml  sql opengl qt3support 

include ( ../../libs-targetfix.pro )
