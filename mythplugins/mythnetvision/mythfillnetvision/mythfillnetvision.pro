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

HEADERS += ../mythnetvision/grabbermanager.h ../mythnetvision/parse.h
HEADERS += ../mythnetvision/rssmanager.h ../mythnetvision/rssdbutil.h
HEADERS += ../mythnetvision/treedbutil.h ../mythnetvision/netutils.h

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += ../mythnetvision/treedbutil.cpp ../mythnetvision/netutils.cpp
SOURCES += ../mythnetvision/rssdbutil.cpp ../mythnetvision/rssmanager.cpp
SOURCES += ../mythnetvision/grabbermanager.cpp ../mythnetvision/parse.cpp main.cpp

include ( ../../libs-targetfix.pro )
