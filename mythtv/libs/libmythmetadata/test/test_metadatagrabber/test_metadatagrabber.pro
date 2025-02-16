include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += network xml sql widgets testlib
using_opengl: QT += opengl

TEMPLATE = app
TARGET = test_metadatagrabber
INCLUDEPATH += ../../..

LIBS += ../../$(OBJECTS_DIR)lyricsdata.o

# Add all the necessary libraries
LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION
LIBS += -L../../../libmythui -lmythui-$$LIBVERSION
LIBS += -L../../../libmythupnp -lmythupnp-$$LIBVERSION
LIBS += -L../../../libmythtv -lmythtv-$$LIBVERSION
LIBS += -L../../../libmyth -lmyth-$$LIBVERSION
LIBS += -L../../../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../../../external/FFmpeg/libswscale -lmythswscale
LIBS += -L../../../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../../../../external/FFmpeg/libavutil -lmythavutil
LIBS += -L../../../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../../../external/FFmpeg/libavfilter -lmythavfilter
LIBS += -L../../../../external/FFmpeg/libpostproc -lmythpostproc
LIBS += -L../.. -lmythmetadata-$$LIBVERSION


using_system_libexiv2 {
LIBS += -lexiv2
} else {
LIBS += -L../../../../external/libexiv2 -lmythexiv2-0.28
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/libexiv2
}

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavutil
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswscale
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavformat
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavcodec
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavfilter
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libpostproc
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswresample
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmyth
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythtv
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythui
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythupnp
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythfreemheg
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythmetadata

# Input
HEADERS += test_metadatagrabber.h
SOURCES += test_metadatagrabber.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
