include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythsamplerate-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

INCLUDEPATH += ../ ../../ 

#build position independent code since the library is linked into a shared library
QMAKE_CFLAGS += -fPIC -DPIC

DEFINES += HAVE_AV_CONFIG_H

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += samplerate.h

SOURCES += samplerate.c src_linear.c src_sinc.c src_zoh.c

inc.path = $${PREFIX}/include/mythtv/
inc.files = samplerate.h

INSTALLS += inc

include ( ../libs-targetfix.pro )
