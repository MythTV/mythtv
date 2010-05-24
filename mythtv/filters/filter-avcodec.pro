# A few filters use routines from libavcodec. Include this in their .pro file

INCLUDEPATH += ../../libs/libavcodec ../../libs/libavutil ../../libs ../..
DEPENDPATH  += ../../libs/libavcodec ../../libs/libavutil

LIBS += -L../../libs/libavutil -lmythavutil-$${LIBVERSION}
LIBS += -L../../libs/libavcodec -lmythavcodec-$${LIBVERSION}

# Rebuild (link) this filter if the lib changes
TARGETDEPS += ../../libs/libavutil/libmythavutil-$${MYTH_SHLIB_EXT}
TARGETDEPS += ../../libs/libavcodec/libmythavcodec-$${MYTH_SHLIB_EXT}
