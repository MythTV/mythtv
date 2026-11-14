include (../../settings.pro)
include (../../version.pro)
include ( ../programs-libs.pro )

QT += network xml sql core

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
HEADERS += mythexternrecorder_commandlineparser.h
HEADERS += stdinnotifier.h
HEADERS += config_base.h
HEADERS += config_ini.h
HEADERS += posixprocess.h
HEADERS += process.h
HEADERS += recorder.h
HEADERS += touchmonitor.h

SOURCES += mythexternrecorder_commandlineparser.cpp
SOURCES += config_base.cpp
SOURCES += config_ini.cpp
SOURCES += main.cpp
SOURCES += posixprocess.cpp
SOURCES += process.cpp
SOURCES += recorder.cpp
SOURCES += stdinnotifier.cpp
SOURCES += touchmonitor.cpp

using_tomlplusplus {
  HEADERS += config_toml.h
  SOURCES += config_toml.cpp
}
