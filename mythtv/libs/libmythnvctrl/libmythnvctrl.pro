include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythnvctrl-$$LIBVERSION
CONFIG += staticlib warn_off
CONFIG -= qt

INCLUDEPATH += ../ ../../ 

#build position independent code since the library is linked into a shared library
QMAKE_CFLAGS += -fPIC -DPIC

DEFINES += HAVE_AV_CONFIG_H

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += nv_control.h  NVCtrl.h  NVCtrlLib.h
SOURCES += NVCtrl.c

include ( ../libs-targetfix.pro )
