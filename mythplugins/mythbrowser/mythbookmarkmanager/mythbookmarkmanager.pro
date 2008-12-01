include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += xml sql

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythbookmarkmanager
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += bookmarkmanager.h bookmarkeditor.h browserdbutil.h
SOURCES += main.cpp bookmarkmanager.cpp bookmarkeditor.cpp browserdbutil.cpp

include ( ../../libs-targetfix.pro )
