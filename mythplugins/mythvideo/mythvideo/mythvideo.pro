include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythvideo

LIBS += -lmythmetadata-$$LIBVERSION  

target.path = $${LIBDIR}/mythtv/plugins

installscripts.path = $${PREFIX}/share/mythtv/mythvideo/scripts
installscripts.files = scripts/*.pl scripts/*.py scripts/README scripts/jamu-example.conf scripts/jamu.README
installscriptsjamumods.path = $${PREFIX}/share/mythtv/mythvideo/scripts/ttvdb                                           
installscriptsjamumods.files = scripts/Television/ttvdb/*.py scripts/Television/ttvdb/*.conf
installscriptmodules.path = $${PREFIX}/share/mythtv/mythvideo/scripts/Movie/MythTV
installscriptmodules.files = scripts/MythTV/MythVideoCommon.pm

INSTALLS += installscripts installscriptsjamumods installscriptmodules target

# Input

HEADERS += videofilter.h dbcheck.h
HEADERS += globalsettings.h fileassoc.h editmetadata.h
HEADERS += videodlg.h videopopups.h videolist.h
HEADERS += playercommand.h playersettings.h
HEADERS += metadatasettings.h

SOURCES += main.cpp videofilter.cpp dbcheck.cpp
SOURCES += globalsettings.cpp fileassoc.cpp editmetadata.cpp
SOURCES += videodlg.cpp videopopups.cpp videolist.cpp
SOURCES += playercommand.cpp playersettings.cpp metadatasettings.cpp

mingw:DEFINES += USING_MINGW

QT += sql xml network

include ( ../../libs-targetfix.pro )
