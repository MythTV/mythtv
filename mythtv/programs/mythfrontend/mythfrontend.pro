include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythfrontend
target.path = $${PREFIX}/bin
INSTALLS = target

setting.path = $${PREFIX}/share/mythtv/
setting.files += theme.txt mysql.txt
setting.files += info_menu.xml mainmenu.xml media_settings.xml tv_schedule.xml
setting.files += util_menu.xml info_settings.xml main_settings.xml
setting.files += recpriorities_settings.xml tv_search.xml tv_lists.xml
setting.files += library.xml manage_recordings.xml optical_menu.xml tvmenu.xml
setting.files += tv_settings.xml
setting.extra = -ldconfig

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += manualbox.h playbackbox.h viewscheduled.h globalsettings.h
HEADERS += manualschedule.h programrecpriority.h channelrecpriority.h
HEADERS += statusbox.h customrecord.h

SOURCES += main.cpp manualbox.cpp playbackbox.cpp viewscheduled.cpp
SOURCES += globalsettings.cpp manualschedule.cpp programrecpriority.cpp 
SOURCES += channelrecpriority.cpp statusbox.cpp customrecord.cpp

macx {
    RC_FILE += mythfrontend.icns
    LIBS += `freetype-config --libs`
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}

using_xvmc:DEFINES += USING_XVMC
using_xvmc_vld:DEFINES += USING_XVMC_VLD
using_xrandr:DEFINES += USING_XRANDR
using_opengl_vsync:DEFINES += USING_OPENGL_VSYNC
