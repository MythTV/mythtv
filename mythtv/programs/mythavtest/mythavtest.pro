include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

TEMPLATE = app
CONFIG += thread
TARGET = mythavtest
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += commandlineparser.h

SOURCES += main.cpp commandlineparser.cpp

macx {
    mac_bundle {
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythavtest
    }
}

using_x11:DEFINES += USING_X11
using_xv:DEFINES += USING_XV
using_ivtv:DEFINES += USING_IVTV
using_xrandr:DEFINES += USING_XRANDR
using_opengl:QT += opengl
using_opengl_video:DEFINES += USING_OPENGL_VIDEO
using_vdpau:DEFINES += USING_VDPAU

using_pulse:DEFINES += USING_PULSE
using_alsa:DEFINES += USING_ALSA
using_jack:DEFINES += USING_JACK
using_oss: DEFINES += USING_OSS
macx:      DEFINES += USING_COREAUDIO
