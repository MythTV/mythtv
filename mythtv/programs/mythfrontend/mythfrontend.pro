INCLUDEPATH += ../../libs/libmythtv ../../libs ../../libs/libmyth
DEPENDPATH += ../../libs/libmythtv ../../libs/libmyth ../../libs/libvbitext
DEPENDPATH += ../../libs/libavcodec ../../libs/libavformat

LIBS += -L../../libs/libmythtv -L../../libs/libavcodec -L../../libs/libmyth
LIBS += -L../../libs/libvbitext -L../../libs/libavformat

include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythfrontend
target.path = $${PREFIX}/bin
INSTALLS = target

setting.path = $${PREFIX}/share/mythtv/
setting.files += theme.txt mysql.txt mainmenu.xml tvmenu.xml tv_settings.xml
setting.files += tv_schedule.xml main_settings.xml
setting.extra = -ldconfig

INSTALLS += setting

LIBS += -lmythtv -lavformat -lavcodec -lvbitext -lmyth-$$LIBVERSION
LIBS += $$EXTRA_LIBS -lmp3lame 

TARGETDEPS += ../../libs/libmythtv/libmythtv.a
TARGETDEPS += ../../libs/libavcodec/libavcodec.a
TARGETDEPS += ../../libs/libvbitext/libvbitext.a
TARGETDEPS += ../../libs/libavformat/libavformat.a

# Input
HEADERS += manualbox.h playbackbox.h viewscheduled.h globalsettings.h
HEADERS += manualschedule.h 

SOURCES += main.cpp manualbox.cpp playbackbox.cpp viewscheduled.cpp
SOURCES += globalsettings.cpp manualschedule.cpp
