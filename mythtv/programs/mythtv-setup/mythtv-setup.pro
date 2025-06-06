include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql widgets

TEMPLATE = app
CONFIG += thread
TARGET = mythtv-setup
target.path = $${PREFIX}/bin

INSTALLS = target

menu.path = $${PREFIX}/share/mythtv/
menu.files += setup.xml

INSTALLS += menu

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += backendsettings.h   channeleditor.h   checksetup.h
HEADERS += exitprompt.h        importicons.h     startprompt.h
HEADERS += expertsettingseditor.h mythtv-setup_commandlineparser.h

SOURCES += backendsettings.cpp channeleditor.cpp checksetup.cpp
SOURCES += exitprompt.cpp      importicons.cpp   startprompt.cpp
SOURCES += mythtv-setup.cpp mythtv-setup_commandlineparser.cpp

macx {
    mac_bundle {
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythtv-setup
    }
}

