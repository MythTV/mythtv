include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

# Input
SOURCES += filter_quickdnr.c
QMAKE_CFLAGS += -fno-strict-aliasing
