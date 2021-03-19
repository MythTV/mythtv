include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

QT += network xml sql core
contains(QT_MAJOR_VERSION, 5): QT += script

TEMPLATE = app
CONFIG += thread
target.files = mythexternrecorder
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

config.path = $${PREFIX}/share/mythtv/externrecorder
config.files += config/*.conf

#config.files += config/ffmpeg-channels.conf config/ffmpeg.conf
#config.files += config/twitch-channels.conf config/twitch.conf
#config.files += config/vlc-channels.conf config/vlc.conf

INSTALLS += config

# Input
HEADERS += commandlineparser.h
HEADERS += MythExternControl.h
HEADERS += MythExternRecApp.h

SOURCES += commandlineparser.cpp
SOURCES += MythExternControl.cpp
SOURCES += MythExternRecApp.cpp
SOURCES += main.cpp

