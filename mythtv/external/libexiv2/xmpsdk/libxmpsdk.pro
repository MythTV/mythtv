include ( ../../../settings.pro )

TEMPLATE = lib
TARGET = mythxmp-0.28

target.path = $${LIBDIR}

INCLUDEPATH += ./include
INCLUDEPATH += ./src
INCLUDEPATH -= /usr/include/libxml2
INCLUDEPATH -= /usr/include/X11
INCLUDEPATH -= /usr/include/drm

QMAKE_CLEAN += $(TARGET)

SOURCES += xmpsdk/src/ExpatAdapter.cpp
SOURCES += xmpsdk/src/MD5.cpp
SOURCES += xmpsdk/src/ParseRDF.cpp
SOURCES += xmpsdk/src/UnicodeConversions.cpp
SOURCES += xmpsdk/src/UnicodeInlines.incl_cpp
SOURCES += xmpsdk/src/WXMPIterator.cpp
SOURCES += xmpsdk/src/WXMPMeta.cpp
SOURCES += xmpsdk/src/WXMPUtils.cpp
SOURCES += xmpsdk/src/XML_Node.cpp
SOURCES += xmpsdk/src/XMPCore_Impl.cpp
SOURCES += xmpsdk/src/XMPIterator.cpp
SOURCES += xmpsdk/src/XMPMeta-GetSet.cpp
SOURCES += xmpsdk/src/XMPMeta-Parse.cpp
SOURCES += xmpsdk/src/XMPMeta-Serialize.cpp
SOURCES += xmpsdk/src/XMPMeta.cpp
SOURCES += xmpsdk/src/XMPUtils-FileInfo.cpp
SOURCES += xmpsdk/src/XMPUtils.cpp
