include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network xml sql

TEMPLATE = app
CONFIG += thread
CONFIG -= moc
TARGET = mythfillnetvision
target.path = $${PREFIX}/bin
INSTALLS = target

INCLUDEPATH += ../mythnetvision

HEADERS += ../mythnetvision/netgrabbermanager.h ../mythnetvision/rssparse.h
HEADERS += ../mythnetvision/mythrssmanager.h ../mythnetvision/netutils.h

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += ../mythnetvision/netutils.cpp ../mythnetvision/mythrssmanager.cpp
SOURCES += ../mythnetvision/netgrabbermanager.cpp ../mythnetvision/rssparse.cpp main.cpp

include ( ../../libs-targetfix.pro )
