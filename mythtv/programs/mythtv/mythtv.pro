include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql opengl

TEMPLATE = app
CONFIG += thread
TARGET = mythtv
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp

macx {
    mac_bundle {
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythtv
    }
}

using_x11:DEFINES += USING_X11
