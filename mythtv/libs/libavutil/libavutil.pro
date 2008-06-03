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

# Debug mode on x86 must compile without -fPIC
# otherwise gcc runs out of registers.
debug:contains(ARCH_X86_32, yes) {
        QMAKE_CFLAGS_SHLIB = 
}

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
SOURCES += adler32.c mathematics.c integer.c lls.c log.c mem.c 
SOURCES += rational.c intfloat_readwrite.c crc.c des.c md5.c fifo.c
SOURCES += aes.c tree.c lzo.c base64.c random.c rc4.c sha1.c string.c

inc.path = $${PREFIX}/include/mythtv/ffmpeg/
inc.files  = adler32.h avutil.h common.h mathematics.h integer.h internal.h 
inc.files += lls.h log.h rational.h intfloat_readwrite.h crc.h md5.h x86_cpu.h
inc.files += softfloat.h fifo.h aes.h tree.h lzo.h base64.h random.h mem.h
inc.files += sha1.h avstring.h

INSTALLS += inc

contains( ARCH_ALPHA, yes ) {
    QMAKE_CFLAGS_RELEASE += -fforce-addr -freduce-all-givs
}

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
