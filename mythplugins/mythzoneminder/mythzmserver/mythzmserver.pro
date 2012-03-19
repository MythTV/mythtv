include ( ../../mythconfig.mak )
include (../../settings.pro )

TEMPLATE = app

CONFIG -= qt

TARGET = mythzmserver
target.path = $${PREFIX}/bin

INSTALLS = target

macx {
    CONFIG += qt
    QT += sql
    #Can't use mysql_config output, as it could have been compiled with
    # universal support, and we may want just 32 or 64 bits
    REGEX="'s/-arch +[a-z0-9_]+ ?//g'"
    QMAKE_LIBS += $$system(mysql_config --libs | sed -E $$REGEX)
} else {
    QMAKE_LIBS += $$system(mysql_config --libs)
}

linux: DEFINES += linux

# Input
HEADERS += zmserver.h

SOURCES += main.cpp zmserver.cpp
