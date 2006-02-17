include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythcontrols
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = controls-ui.xml ../images/kb-button-on.png ../images/kb-button-off.png
installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = controls-ui.xml

INSTALLS += uifiles

# Input
HEADERS += action.h actionid.h mythcontrols.h keybindings.h keygrabber.h
SOURCES += action.cpp actionset.cpp keybindings.cpp mythcontrols.cpp
SOURCES += keygrabber.cpp main.cpp 

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
