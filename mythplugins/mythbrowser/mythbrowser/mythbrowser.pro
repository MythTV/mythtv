include (../../mythconfig.mak )
include (../../settings.pro )
include (../../programs-libs.pro )

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythbrowser
target.path = $${PREFIX}/bin
INSTALLS += target

installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installimages

# Input
HEADERS += mythbrowser.h webpage.h 
SOURCES += main.cpp mythbrowser.cpp webpage.cpp

#The following line was inserted by qt3to4
QT +=  network xml sql opengl qt3support webkit
