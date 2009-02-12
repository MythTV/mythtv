include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythswscale-$$LIBVERSION
CONFIG += thread dll warn_off
CONFIG -= qt
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_LFLAGS += $$LDFLAGS

INCLUDEPATH = .. ../..

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
SOURCES += rgb2rgb.c swscale.c swscale_avoption.c

contains( ARCH_BFIN, yes ) {
        SOURCES +=  swscale_bfin.c yuv2rgb_bfin.c internal_bfin.S
}

contains( CONFIG_GPL, yes ) {
        SOURCES +=  yuv2rgb.c
}

contains( CONFIG_MLIB, yes )  {
        SOURCES +=  yuv2rgb_mlib.c
}

contains( HAVE_ALTIVEC, yes ) {
        SOURCES +=  yuv2rgb_altivec.c
}

contains( HAVE_VIS, yes )  {
        SOURCES +=  yuv2rgb_vis.c
}

inc.path = $${PREFIX}/include/mythtv/libswscale/
inc.files  = swscale.h rgb2rgb.h

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
    LIBS               += -lz
    QMAKE_LFLAGS_SHLIB += -single_module
    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC2000000
}

include ( ../libs-targetfix.pro )
