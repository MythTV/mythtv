include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs/libmythtv ../../libs/libavcodec ../..

# Input
SOURCES += filter_yadif.c

contains(ARCH_X86, yes) {
    SOURCES += aclib.c
}
