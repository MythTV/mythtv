include ( ../../mythconfig.mak )

CONFIG -= qt

TARGET = mythzmserver
target.path = $${PREFIX}/bin

INSTALLS = target

LIBS = -lmysqlclient -L$${LIBDIR}/mysql

# Input
HEADERS += zmserver.h

SOURCES += main.cpp zmserver.cpp
