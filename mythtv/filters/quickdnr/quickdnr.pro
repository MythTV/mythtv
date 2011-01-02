include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

# Input
SOURCES += filter_quickdnr.c
QMAKE_CXXFLAGS += -fno-strict-aliasing
