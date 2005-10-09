include ( ../../config.mak )
include ( ../../settings.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = mythlcdserver
target.path = $${PREFIX}/bin

INSTALLS += target

HEADERS += lcdserver.h serversocket.h lcdprocclient.h

SOURCES += main.cpp lcdserver.cpp serversocket.cpp lcdprocclient.cpp 

