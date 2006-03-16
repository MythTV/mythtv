include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavutil-$$LIBVERSION
CONFIG += thread dll warn_off
CONFIG -= qt
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH = ../ ../../

DEFINES += HAVE_AV_CONFIG_H _LARGEFILE_SOURCE

# Debug mode on x86 must compile without -fPIC and with -O, 
# otherwise gcc runs out of registers.
debug:contains(TARGET_ARCH_X86, yes) {
    QMAKE_CFLAGS_SHLIB = 
}

QMAKE_CFLAGS_DEBUG += -O

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
SOURCES += mathematics.c integer.c rational.c intfloat_readwrite.c crc.c

inc.path = $${PREFIX}/include/mythtv/ffmpeg/
inc.files = avutil.h common.h mathematics.h integer.h rational.h intfloat_readwrite.h crc.h

INSTALLS += inc

contains( TARGET_ARCH_ALPHA, yes ) {
    QMAKE_CFLAGS_RELEASE += -fforce-addr -freduce-all-givs
}

contains( TARGET_ALTIVEC, yes ) {
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
    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC3000000
}

