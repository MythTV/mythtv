include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs/libmythtv ../../libs/libavcodec ../..

# Input
SOURCES += filter_yadif.c aclib.c
