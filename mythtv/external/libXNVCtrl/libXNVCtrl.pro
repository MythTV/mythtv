include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythXNVCtrl-$${LIBVERSION}
CONFIG += staticlib warn_off
CONFIG -= qt

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += nv_control.h  NVCtrl.h  NVCtrlLib.h
SOURCES += NVCtrl.c

