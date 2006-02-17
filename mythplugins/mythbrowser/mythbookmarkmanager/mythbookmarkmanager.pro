include ( ../../mythconfig.mak )
include ( ../../settings.pro )

INCLUDEPATH += /usr/include/kde
INCLUDEPATH += /opt/kde3/include

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythbookmarkmanager
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

#uifiles.path = $${PREFIX}/share/mythtv/themes/default
#uifiles.files = news-ui.xml
#installfiles.path = $${PREFIX}/share/mythtv/mythnews
#installfiles.files = news-sites.xml
#installimages.path = $${PREFIX}/share/mythtv/themes/default
#installimages.files = images/*.png

#INSTALLS += installfiles installimages uifiles

# Input
HEADERS += bookmarkmanager.h
SOURCES += main.cpp bookmarkmanager.cpp
