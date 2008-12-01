include (../../mythconfig.mak )
include (../../settings.pro )
include (../../programs-libs.pro )

QT += network xml sql opengl webkit

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythbrowser
target.path = $${PREFIX}/bin
INSTALLS += target

installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installimages

INCLUDEPATH += ../mythbookmarkmanager

DEFINES += MYTHBROWSER_STANDALONE

# Input
HEADERS += mythbrowser.h webpage.h ../mythbookmarkmanager/bookmarkeditor.h
HEADERS += ../mythbookmarkmanager/bookmarkmanager.h ../mythbookmarkmanager/browserdbutil.h
SOURCES += main.cpp mythbrowser.cpp webpage.cpp ../mythbookmarkmanager/bookmarkeditor.cpp
SOURCES += ../mythbookmarkmanager/bookmarkmanager.cpp ../mythbookmarkmanager/browserdbutil.cpp
