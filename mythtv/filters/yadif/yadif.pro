include ( ../filter-common.pro )
include ( ../filter-avcodec.pro )

INCLUDEPATH += ../../libs/libmythtv ../../libs/libavcodec ../..
mingw:LIBS += $$EXTRA_LIBS

# Input
SOURCES += filter_yadif.c

contains(ARCH_X86, yes) {
    SOURCES += aclib.c
}

macx:debug:DEFINES -= MMX
