include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib widgets

TEMPLATE = app
TARGET = test_mheg_dsmcc
INCLUDEPATH += ../../..

# Directly add objects w/non-exported functions
LIBS += ../../$(OBJECTS_DIR)dsmccbiop.o
LIBS += ../../$(OBJECTS_DIR)dsmcccache.o
LIBS += ../../$(OBJECTS_DIR)dsmcc.o
LIBS += ../../$(OBJECTS_DIR)dsmccobjcarousel.o

# Add all the necessary libraries
LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION
LIBS += -L../../../libmythui -lmythui-$$LIBVERSION
LIBS += -L../../../libmythupnp -lmythupnp-$$LIBVERSION
LIBS += -L../../../libmyth -lmyth-$$LIBVERSION
LIBS += -L../../../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../../../external/FFmpeg/libswscale -lmythswscale
LIBS += -L../../../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../../../../external/FFmpeg/libavutil -lmythavutil
LIBS += -L../../../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../../../external/FFmpeg/libavfilter -lmythavfilter
LIBS += -L../../../../external/FFmpeg/libpostproc -lmythpostproc
using_mheg:LIBS += -L../../../libmythfreemheg -lmythfreemheg-$$LIBVERSION
LIBS += -L../.. -lmythtv-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavutil
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswscale
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavformat
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavcodec
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavfilter
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libpostproc
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswresample
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmyth
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythui
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythupnp
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythfreemheg

# Input
HEADERS += test_mheg_dsmcc.h
SOURCES += test_mheg_dsmcc.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
