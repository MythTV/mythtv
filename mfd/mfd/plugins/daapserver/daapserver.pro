include (../../../settings.pro)

INCLUDEPATH += ../../../mfdlib

TEMPLATE = lib
CONFIG += plugin thread

QMAKE_CXXFLAGS_RELEASE  += -Wno-multichar 
QMAKE_CXXFLAGS_DEBUG    += -Wno-multichar

TARGET = mfdplugin-daapserver

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

#HEADERS += ./daapd/types.h     \
#           ./daapd/dboutput.h  \
#           ./daapd/parsemp3.h  \
#           ./daapd/songcache.h      

#SOURCES += ./daapd/db.cc       \
#           ./daapd/dboutput.cc \
#           ./daapd/parsemp3.cc \
#           ./daapd/songcache.cc

HEADERS +=          daapserver.h   request.h   session.h   mcc_bitfield.h
SOURCES += main.cpp daapserver.cpp request.cpp session.cpp

LIBS += -L./daaplib/ -ldaaplib -lid3tag -lz -L./httpd_persistent -lhttpd_persistent

