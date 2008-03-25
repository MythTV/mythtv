include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythtvosd
target.path = $${PREFIX}/bin
INSTALLS = target

LIBS += $$EXTRA_LIBS 
INCLUDEPATH += ../../libs/libmyth

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp

#The following line was inserted by qt3to4
QT += network xml  sql opengl qt3support
