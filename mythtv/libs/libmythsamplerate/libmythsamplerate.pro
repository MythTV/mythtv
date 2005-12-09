include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythsamplerate-$$LIBVERSION
CONFIG += thread staticlib warn_off
target.path = $${PREFIX}/lib
INSTALLS = target

INCLUDEPATH += ../../ 

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -DHAVE_AV_CONFIG_H -I.. -fPIC -DPIC -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -DHAVE_AV_CONFIG_H -I.. -fPIC -DPIC -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += samplerate.h

SOURCES += samplerate.c src_linear.c src_sinc.c src_zoh.c

inc.path = $${PREFIX}/include/mythtv/
inc.files = samplerate.h

INSTALLS += inc
