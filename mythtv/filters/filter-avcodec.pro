# A few filters use routines from libavcodec. Include this in their .pro file
include(../settings.pro)

LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavcore  -lmythavcore
LIBS += -L../../external/FFmpeg/libavutil  -lmythavutil

# Rebuild (link) this filter if the lib changes
POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
POST_TARGETDEPS += ../../external/FFmpeg/libavcore/$$avLibName(avcore)
POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
