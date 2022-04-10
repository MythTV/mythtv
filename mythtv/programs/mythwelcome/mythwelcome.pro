include (../../settings.pro)
include (../../version.pro)
include (../programs-libs.pro)

QT += xml sql network widgets

TEMPLATE = app
CONFIG += thread
TARGET = mythwelcome
target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += welcomedialog.h welcomesettings.h mythwelcome_commandlineparser.h
SOURCES += mythwelcome.cpp welcomedialog.cpp welcomesettings.cpp mythwelcome_commandlineparser.cpp

macx {
    mac_bundle {
        CONFIG -= console  # Force behaviour of producing .app bundle
        RC_FILE += mythfrontend.icns
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythwelcome.app
    }
}

win32 : !debug {
    # To hide the window that contains logging output:
    CONFIG -= console
    DEFINES += WINDOWS_CLOSE_CONSOLE
}
