include ( ../mythconfig.mak )
include ( ../settings.pro )
 
INCLUDEPATH += $${PREFIX}/include/mythtv

TEMPLATE = lib opengl
CONFIG += plugin thread
TARGET = mytharchive
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

menufiles.path = $${PREFIX}/share/mythtv
menufiles.files = archivemenu.xml archiveselect.xml

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = mytharchive-ui.xml ../images/*.png

scriptfiles.path = $${PREFIX}/share/mythtv/mytharchive/scripts
scriptfiles.files = ../mythburn/scripts/*

introfiles.path = $${PREFIX}/share/mythtv/mytharchive/intro
introfiles.files = ../mythburn/intro/*.mpg

musicfiles.path = $${PREFIX}/share/mythtv/mytharchive/music
musicfiles.files = ../mythburn/music/*.mp2

imagefiles.path = $${PREFIX}/share/mythtv/mytharchive/images
imagefiles.files = ../mythburn/images/*.png

themefiles.path = $${PREFIX}/share/mythtv/mytharchive/themes
themefiles.files = ../mythburn/themes/*

profilefiles.path = $${PREFIX}/share/mythtv/mytharchive/encoder_profiles
profilefiles.files = ../mythburn/encoder_profiles/*.xml

#fontfiles.path = $${PREFIX}/share/mythtv
#fontfiles.files = ../fonts/*.ttf

INSTALLS += menufiles uifiles scriptfiles introfiles themefiles imagefiles musicfiles profilefiles

HEADERS += archivesettings.h mythburnwizard.h logviewer.h fileselector.h
HEADERS += recordingselector.h videoselector.h dbcheck.h editmetadata.h
HEADERS += advancedoptions.h mytharchivewizard.h mythnativewizard.h importnativewizard.h

SOURCES += main.cpp archivesettings.cpp mythburnwizard.cpp logviewer.cpp 
SOURCES += fileselector.cpp recordingselector.cpp videoselector.cpp
SOURCES += dbcheck.cpp editmetadata.cpp advancedoptions.cpp 
SOURCES += mytharchivewizard.cpp mythnativewizard.cpp importnativewizard.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

