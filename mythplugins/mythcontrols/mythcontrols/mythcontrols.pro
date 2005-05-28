include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythcontrols
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = controls-ui.xml
installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = controls-ui.xml

INSTALLS += uifiles

CFLAGS += -I$${PREFIX}/include

# Input
HEADERS += action.h keyconflict.h actionidentifier.h mythcontrols.h keybindings.h keygrabber.h conflictresolver.h
SOURCES += main.cpp action.cpp keyconflict.cpp mythcontrols.cpp keybindings.cpp keygrabber.cpp conflictresolver.cpp

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
