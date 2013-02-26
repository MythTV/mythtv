include ( ../../settings.pro)
include ( ../../version.pro)
include ( ../programs-libs.pro)

QT += sql network
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

CONFIG -= opengl

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += commandlineparser.h

SOURCES += main.cpp commandlineparser.cpp
