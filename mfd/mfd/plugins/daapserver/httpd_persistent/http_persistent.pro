
TEMPLATE = lib
CONFIG   = staticlib qt

TARGET   = httpd_persistent

HEADERS += config.h httpd.h httpd_priv.h select.h
SOURCES += api.cpp ip_acl.cpp protocol.cpp select.cpp version.cpp



