include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythbookmarkmanager
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

# Input
HEADERS += bookmarkmanager.h bookmarkeditor.h browserdbutil.h
SOURCES += main.cpp bookmarkmanager.cpp bookmarkeditor.cpp browserdbutil.cpp

#The following line was inserted by qt3to4
QT += xml  sql opengl qt3support 

include ( ../../libs-targetfix.pro )
