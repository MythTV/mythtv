include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythavutil-$$LIBVERSION
CONFIG += thread dll warn_off
CONFIG -= qt
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_LFLAGS += $$LDFLAGS

INCLUDEPATH = .. ../..

DEFINES += HAVE_AV_CONFIG_H

# Debug mode on x86 must compile without -fPIC
# otherwise gcc runs out of registers.
debug:contains(ARCH_X86_32, yes) {
        QMAKE_CFLAGS -= -fPIC -DPIC
}

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
SOURCES += adler32.c
SOURCES += aes.c
SOURCES += avstring.c
SOURCES += base64.c
SOURCES += crc.c
SOURCES += des.c
SOURCES += fifo.c
SOURCES += integer.c
SOURCES += intfloat_readwrite.c
SOURCES += lfg.c
SOURCES += lls.c
SOURCES += log.c
SOURCES += lzo.c
SOURCES += mathematics.c
SOURCES += md5.c
SOURCES += mem.c
SOURCES += pixdesc.c
SOURCES += random_seed.c
SOURCES += rational.c
SOURCES += rc4.c
SOURCES += sha.c
SOURCES += tree.c
SOURCES += utils.c

inc.path = $${PREFIX}/include/mythtv/libavutil/
inc.files  = adler32.h
inc.files  = avconfig.h
inc.files += avstring.h
inc.files += avutil.h
inc.files += base64.h
inc.files += common.h
inc.files += crc.h
inc.files += fifo.h
inc.files += intfloat_readwrite.h
inc.files += log.h
inc.files += lzo.h
inc.files += mathematics.h
inc.files += md5.h
inc.files += mem.h
inc.files += pixdesc.h
inc.files += pixfmt.h
inc.files += rational.h
inc.files += sha1.h

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

include ( ../libs-targetfix.pro )
