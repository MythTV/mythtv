include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

QT += network xml sql

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythtv-setup
target.path = $${PREFIX}/bin

INSTALLS = target

menu.path = $${PREFIX}/share/mythtv/
menu.files += setup.xml

INSTALLS += menu

QMAKE_CLEAN += $(TARGET)

using_backend {
    DEFINES += USING_BACKEND
}

# Input
HEADERS += backendsettings.h   checksetup.h   exitprompt.h importicons.h
HEADERS += channeleditor.h
SOURCES += backendsettings.cpp checksetup.cpp exitprompt.cpp main.cpp
SOURCES += importicons.cpp channeleditor.cpp

macx {
    mac_bundle {
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythtv-setup
    }
}

using_x11:DEFINES += USING_X11

