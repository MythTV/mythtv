include ( ../../mythconfig.mak )
include ( ../../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythcontrols
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv

# Input
HEADERS += action.h actionid.h mythcontrols.h keybindings.h keygrabber.h
SOURCES += action.cpp actionset.cpp keybindings.cpp mythcontrols.cpp
SOURCES += keygrabber.cpp main.cpp 

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
#The following line was inserted by qt3to4
QT += sql xml opengl qt3support
