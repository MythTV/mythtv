include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

QT += network xml sql script core

TEMPLATE = app
CONFIG += thread
target.files = mythexternrecorder
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

config.path = $${PREFIX}/share/mythtv/
config.files += ffmpeg-channels.conf ffmpeg.conf
config.files += twitch-channels.conf twitch.conf
config.files += vlc-channels.conf vlc.conf

INSTALLS += config

# Input
HEADERS += commandlineparser.h
HEADERS += MythExternControl.h
HEADERS += MythExternRecApp.h

SOURCES += commandlineparser.cpp
SOURCES += MythExternControl.cpp
SOURCES += MythExternRecApp.cpp
SOURCES += main.cpp

