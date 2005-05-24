# A few filters use routines from libavcodec. Include this in their .pro file

# This eliminates a linker warning
CONFIG += qt

INCLUDEPATH += ../../libs/libavcodec
DEPENDPATH  += ../../libs/libavcodec

LIBS += -L../../libs/libavcodec -lmythavcodec-$${LIBVERSION}

# Rebuild (link) this filter if the lib changes
isEmpty(QMAKE_EXTENSION_SHLIB) {
    QMAKE_EXTENSION_SHLIB=so
}
TARGETDEPS += ../../libs/libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
