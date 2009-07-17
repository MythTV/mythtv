include (../../settings.pro)
include (../programs-libs.pro)

QT += xml sql

TEMPLATE = app
CONFIG += thread
TARGET = mythwelcome
target.path = $${PREFIX}/bin

INSTALLS = target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = welcome-ui.xml images/*.png

INSTALLS += uifiles 

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += welcomedialog.h welcomesettings.h
SOURCES += main.cpp welcomedialog.cpp welcomesettings.cpp
