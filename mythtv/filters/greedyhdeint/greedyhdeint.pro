include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs/libmythtv ../../libs/libavcodec ../..

# Input
SOURCES += filter_greedyhdeint.c color.c

# llvm/xcode doesn't compile if -O3 is enabled (or any other -O for that matter)
macx: QMAKE_CFLAGS -= -O3 -O2 -O1 -Os
