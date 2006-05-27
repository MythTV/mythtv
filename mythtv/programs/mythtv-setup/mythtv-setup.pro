include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythtv-setup
target.path = $${PREFIX}/bin

INSTALLS = target

menu.path = $${PREFIX}/share/mythtv/
menu.files += setup.xml

INSTALLS += menu

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += backendsettings.h
SOURCES += backendsettings.cpp checksetup.cpp main.cpp

