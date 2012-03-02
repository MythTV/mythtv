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
    MYSQLIBS = "$$system(mysql_config --libs)"
    MYSQLIBS ~= s/-arch +[a-z0-9_]+//g
    LIBS += $$sprintf($$MYSQLIBS) 
} else {
    LIBS += $$system(mysql_config --libs)
}                                                                              

linux: DEFINES += linux

# Input
HEADERS += zmserver.h

SOURCES += main.cpp zmserver.cpp
