include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs/libmythtv ../../libs/libavcodec ../..

# Input
SOURCES += filter_denoise3d.c
