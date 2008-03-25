include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

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
        QMAKE_POST_LINK = ../../contrib/OSX/makebundle.sh mythtv
    }
}

using_x11:DEFINES += USING_X11

#The following line was inserted by qt3to4
QT += network xml  sql opengl qt3support
