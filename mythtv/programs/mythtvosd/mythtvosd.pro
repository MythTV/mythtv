include ( ../../settings.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythtvosd
target.path = $${PREFIX}/bin
INSTALLS = target

# Input
SOURCES += main.cpp
