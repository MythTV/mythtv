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

macx:      DEFINES += USING_COREAUDIO
