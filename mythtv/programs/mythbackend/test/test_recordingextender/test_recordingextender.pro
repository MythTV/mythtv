include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += network sql widgets xml testlib

TEMPLATE = app
TARGET = test_recordingextender
DEPENDPATH += . ../..
INCLUDEPATH += . ../..
INCLUDEPATH += ../../../../libs

LIBS += ../../obj/recordingextender.o
LIBS += ../../obj/moc_recordingextender.o

# Add all the necessary libraries
LIBS += -L../../../../libs/libmythbase -lmythbase-$$LIBVERSION
LIBS += -L../../../../libs/libmythui -lmythui-$$LIBVERSION
LIBS += -L../../../../libs/libmythupnp -lmythupnp-$$LIBVERSION
LIBS += -L../../../../libs/libmyth -lmyth-$$LIBVERSION
LIBS += -L../../../../libs/libmythtv -lmythtv-$$LIBVERSION
LIBS += -L../../../../libs/libmythmetadata -lmythmetadata-$$LIBVERSION
# Add FFMpeg for libmythtv
LIBS += -L../../../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../../../external/FFmpeg/libavutil -lmythavutil
LIBS += -L../../../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../../../external/FFmpeg/libswscale -lmythswscale
LIBS += -L../../../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../../../../external/FFmpeg/libavfilter -lmythavfilter
LIBS += -L../../../../external/FFmpeg/libpostproc -lmythpostproc
using_mheg:LIBS += -L../../../../libs/libmythfreemheg -lmythfreemheg-$$LIBVERSION

using_mheg:QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmythfreemheg
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmythmetadata
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmythtv
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmyth
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmythupnp
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmythui
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavutil
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswscale
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavformat
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavfilter
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavcodec
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libpostproc
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswresample
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../

!using_system_libexiv2 {
    LIBS += -L../../../../external/libexiv2 -lmythexiv2-0.28
    QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/libexiv2 -lexpat
    freebsd: LIBS += -lprocstat -liconv
    darwin: LIBS += -liconv -lz
}

DEFINES += TEST_SOURCE_DIR='\'"$${PWD}"'\'

# Input
HEADERS += test_recordingextender.h
SOURCES += test_recordingextender.cpp dummyscheduler.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
