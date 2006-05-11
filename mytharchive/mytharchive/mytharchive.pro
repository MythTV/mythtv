include ( ../mythconfig.mak )
include ( ../settings.pro )

TEMP = $${SRC_PATH}
TEMP ~= s/'//

INCLUDEPATH += $${TEMP}/libs/libmythtv $${TEMP}/libs/libmyth

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

themefiles.path = $${PREFIX}/share/mythtv/mytharchive/themes
themefiles.files = ../mythburn/themes/*

#fontfiles.path = $${PREFIX}/share/mythtv
#fontfiles.files = ../fonts/*.ttf

INSTALLS += menufiles uifiles scriptfiles introfiles themefiles

HEADERS += archivesettings.h mythburnwizard.h logviewer.h fileselector.h
HEADERS += recordingselector.h videoselector.h dbcheck.h editmetadata.h
HEADERS += advancedoptions.h mytharchivewizard.h mythnativewizard.h

SOURCES += main.cpp archivesettings.cpp mythburnwizard.cpp logviewer.cpp 
SOURCES += fileselector.cpp recordingselector.cpp videoselector.cpp
SOURCES += dbcheck.cpp editmetadata.cpp advancedoptions.cpp 
SOURCES += mytharchivewizard.cpp mythnativewizard.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

