include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}
using_opengl:QT += opengl

TEMPLATE = app
CONFIG += thread
TARGET = mythlcdserver
target.path = $${PREFIX}/bin

INSTALLS += target

HEADERS += lcdserver.h  lcdprocclient.h commandlineparser.h

SOURCES += main.cpp lcdserver.cpp lcdprocclient.cpp commandlineparser.cpp
