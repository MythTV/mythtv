include (../../../../settings.pro)

TEMPLATE = lib
CONFIG += staticlib
TARGET = httpdp


HEADERS += config.h httpd.h httpd_priv.h select.h
SOURCES += api.cpp ip_acl.cpp protocol.cpp select.cpp version.cpp

