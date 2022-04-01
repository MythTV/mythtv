include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql widgets

TEMPLATE = app
CONFIG += thread
TARGET = mythlcdserver
target.path = $${PREFIX}/bin

INSTALLS += target

HEADERS += lcdserver.h  lcdprocclient.h mythlcdserver_commandlineparser.h

SOURCES += mythlcdserver.cpp lcdserver.cpp lcdprocclient.cpp mythlcdserver_commandlineparser.cpp
