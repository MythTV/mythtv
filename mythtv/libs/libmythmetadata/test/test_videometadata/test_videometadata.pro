include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib
using_opengl: QT += opengl

TEMPLATE = app
TARGET = test_videometadata
INCLUDEPATH += ../../..

LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION
LIBS += -L../.. -lmythmetadata-$$LIBVERSION
# libmyth and libmythtv for ProgramInfo and RecordingInfo
LIBS += -L../../../libmyth -lmyth-$$LIBVERSION
LIBS += -L../../../libmythtv -lmythtv-$$LIBVERSION
# libmythui for MythUIProgressDialog
LIBS += -L../../../libmythui -lmythui-$$LIBVERSION
LIBS += -L../../../libmythupnp -lmythupnp-$$LIBVERSION
LIBS += -L../../../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../../../external/FFmpeg/libavutil -lmythavutil
LIBS += -L../../../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../../../external/FFmpeg/libswscale -lmythswscale
LIBS += -L../../../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../../../../external/FFmpeg/libavfilter -lmythavfilter
LIBS += -L../../../../external/FFmpeg/libpostproc -lmythpostproc
using_mheg:LIBS += -L../../../libmythfreemheg -lmythfreemheg-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavutil
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswscale
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavformat
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavfilter
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavcodec
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libpostproc
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswresample
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmyth
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythui
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythupnp
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythtv
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythfreemheg

!using_system_libexiv2 {
    LIBS += -L../../../../external/libexiv2 -lmythexiv2-0.28
    QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/libexiv2 -lexpat
    freebsd: LIBS += -lprocstat -liconv
    darwin: LIBS += -liconv -lz
}

# Input
HEADERS += test_videometadata.h
SOURCES += test_videometadata.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking on Ubuntu 17.10.
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
