include ( ../../config.mak )
include (../../settings.pro)
include (../programs-libs.pro)

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin

INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += 
SOURCES += main.cpp

#The following line was inserted by qt3to4
QT += network xml  sql opengl qt3support
