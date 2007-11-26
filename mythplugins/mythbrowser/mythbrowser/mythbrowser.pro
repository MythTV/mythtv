include (../../mythconfig.mak )
include (../../settings.pro )
include (../../programs-libs.pro )

KDEPREFIX = $$SYSTEM(kde-config --prefix)
INCLUDEPATH += $${KDEPREFIX}/include
INCLUDEPATH += $${KDEPREFIX}/include/kde

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythbrowser
target.path = $${PREFIX}/bin
INSTALLS += target

installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installimages

LIBS += -L$$SYSTEM(kde-config --expandvars --install lib) -lkhtml

# Input
HEADERS += webpage.h tabview.h
SOURCES += main.cpp webpage.cpp tabview.cpp
 
