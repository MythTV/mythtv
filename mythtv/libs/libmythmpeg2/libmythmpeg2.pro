include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythmpeg2-$$LIBVERSION
CONFIG += thread staticlib warn_off
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.18.1 

QMAKE_CFLAGS_RELEASE += -fPIC -fno-common
QMAKE_CFLAGS_DEBUG += -fPIC -fno-common

# Input
HEADERS += attributes.h vlc.h mpeg2_internal.h mpeg2.h

SOURCES += cpu_accel.c cpu_state.c \
           alloc.c header.c decode.c slice.c \
           motion_comp.c idct.c

inc.path = $${PREFIX}/include/mythtv/mpeg2dec/
inc.files = mpeg2.h

INSTALLS += inc

contains( TARGET_ALTIVEC, yes ) {
    SOURCES += motion_comp_altivec.c idct_altivec.c
}
contains( TARGET_MMX, yes ) {
    HEADERS += mmx.h
    SOURCES += motion_comp_mmx.c idct_mmx.c
}
contains( TARGET_ARCH_SPARC, yes ) {
    HEADERS += vis.h
    SOURCES += motion_comp_vis.c
}
contains( TARGET_ARCH_ALPHA, yes ) {
    HEADERS += alpha_asm.h
    SOURCES += motion_comp_alpha.c idct_alpha.c
}
