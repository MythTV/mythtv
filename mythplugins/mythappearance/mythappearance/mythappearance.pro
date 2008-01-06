include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythappearance
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = appear-ui.xml


uifiles_wide.path = $${PREFIX}/share/mythtv/themes/default-wide
uifiles_wide.files = theme-wide/appear-ui.xml

installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

installxml.path = $${PREFIX}/share/mythtv
installxml.files = util_menu.xml

INSTALLS += uifiles uifiles_wide installimages installxml

# Input
HEADERS += mythappearance.h
SOURCES += main.cpp mythappearance.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
