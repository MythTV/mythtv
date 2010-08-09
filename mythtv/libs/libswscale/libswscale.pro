include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythswscale-$$LIBVERSION
CONFIG += thread dll warn_off
CONFIG -= qt
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_LFLAGS += $$LDFLAGS

debug:contains(ARCH_X86_32, yes) {
        QMAKE_CFLAGS_DEBUG += -fomit-frame-pointer
}
# gcc-4.2 and newer can not compile with PIC on x86
contains(ARCH_X86_32, yes) {
        QMAKE_CFLAGS -= -fPIC -DPIC
}


!profile:QMAKE_CFLAGS_DEBUG += -O

INCLUDEPATH = .. ../..

DEFINES += HAVE_AV_CONFIG_H

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
SOURCES += options.c rgb2rgb.c swscale.c utils.c yuv2rgb.c

contains( ARCH_BFIN, yes ) {
        SOURCES += bfin/internal_bfin.S
        SOURCES += bfin/swscale_bfin.c
        SOURCES += bfin/yuv2rgb_bfin.c
}

contains( CONFIG_MLIB, yes )  {
        SOURCES += mlib/yuv2rgb_mlib.c
}

contains( HAVE_ALTIVEC, yes ) {
        SOURCES += ppc/yuv2rgb_altivec.c
}

contains( HAVE_VIS, yes )  {
        SOURCES += sparc/yuv2rgb_vis.c
}

contains( HAVE_MMX, yes ):contains( CONFIG_GPL, yes )  {
        SOURCES += x86/yuv2rgb_mmx.c
}

inc.path = $${PREFIX}/include/mythtv/libswscale/
inc.files  = swscale.h

INSTALLS += inc

LIBS += -L../libavutil -lmythavutil-$$LIBVERSION -lm

contains( HAVE_ALTIVEC, yes ) {
  macx {
    QMAKE_CFLAGS_RELEASE += -faltivec
    QMAKE_CFLAGS_DEBUG   += -faltivec
  }
  !macx {
    QMAKE_CFLAGS_RELEASE += -maltivec -mabi=altivec
  }
}

macx {
    QMAKE_LFLAGS_SHLIB += -read_only_relocs warning
}

include ( ../libs-targetfix.pro )
