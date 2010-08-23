include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs/libmythtv
DEPENDPATH  += ../../libs/libmythtv

SOURCES += filter_postprocess.c

# For pp_free_context, pp_free_mode, pp_get_context, pp_help:
LIBS += -L../../external/FFmpeg/libpostproc -lmythpostproc
