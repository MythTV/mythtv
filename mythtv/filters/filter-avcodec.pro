# A few filters use routines from libavcodec. Include this in their .pro file

LIBS += -L../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../external/FFmpeg/libavutil  -lmythavutil
LIBS += -L../../external/FFmpeg/libavformat  -lmythavformat

# Rebuild (link) this filter if the lib changes
POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
POST_TARGETDEPS += ../../external/FFmpeg/libavformat/$$avLibName(avformat)
