include (../../../settings.pro)


TEMPLATE = lib
CONFIG += plugin thread
TARGET = mfdplugin-daapserver

target.path = $${PREFIX}/lib/mythtv/mfdplugins
INSTALLS += target

HEADERS += ../../mfdplugin.h   ../../events.h
SOURCES += ../../mfdplugin.cpp ../../events.cpp

HEADERS += ./httpd_persistent/config.h       \
           ./httpd_persistent/httpd.h        \
           ./httpd_persistent/httpd_priv.h   \
           ./httpd_persistent/select.h

SOURCES += ./httpd_persistent/api.cpp        \
           ./httpd_persistent/ip_acl.cpp     \ 
           ./httpd_persistent/protocol.cpp   \
           ./httpd_persistent/select.cpp     \
           ./httpd_persistent/version.cpp


HEADERS +=          daapserver.h
SOURCES += main.cpp daapserver.cpp

