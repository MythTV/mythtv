include (../../../settings.pro)


TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-zeroconfig

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

#
#                   These are defines that apple's mDNS stuff needs
#

DEFINES +=          NOT_HAVE_SA_LEN

#
#   You can set this to 0 (optimize debug statements out of object code)
#                       1 (stderr debugging statements)
#                       2 (really verbose debugging statements)
#

DEFINES +=          MDNS_DEBUGMSGS=0

HEADERS +=          zc_supervisor.h   zc_responder.h   zc_client.h   ../../mfdplugin.h   ../../events.h       \
                    ./apple/mDNSClientAPI.h ./apple/mDNSPosix.h ./apple/mDNSUNP.h ./apple/mDNSDebug.h

SOURCES += main.cpp zc_supervisor.cpp zc_responder.cpp zc_client.cpp ../../mfdplugin.cpp ../../events.cpp     \
                    ./apple/mDNS.c          ./apple/mDNSPosix.c ./apple/mDNSUNP.c  


