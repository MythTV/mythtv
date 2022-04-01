include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql widgets

TEMPLATE = app
CONFIG += thread
TARGET = mythavtest
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mythavtest_commandlineparser.h

SOURCES += mythavtest.cpp mythavtest_commandlineparser.cpp

macx {
    mac_bundle {
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythavtest
    }
}

using_x11:DEFINES += USING_X11
using_opengl:DEFINES += USING_OPENGL
using_vdpau:DEFINES += USING_VDPAU
using_vaapi:using_opengl:DEFINES += USING_VAAPI
using_mmal:DEFINES += USING_MMAL

using_pulse:DEFINES += USING_PULSE
using_alsa:DEFINES += USING_ALSA
using_jack:DEFINES += USING_JACK
using_oss: DEFINES += USING_OSS
macx:      DEFINES += USING_COREAUDIO
