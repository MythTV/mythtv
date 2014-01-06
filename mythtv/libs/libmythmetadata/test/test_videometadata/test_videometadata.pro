include ( ../../../../settings.pro )

QT += xml sql network

contains(QT_VERSION, ^4\\.[0-9]\\..*) {
CONFIG += qtestlib
}
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += testlib
}

TEMPLATE = app
TARGET = test_videometadata
DEPENDPATH += . ../.. ../../../libmythbase ../../../libmythtv ../../../libmyth
INCLUDEPATH += . ../.. ../../../libmythbase ../../../libmythtv ../../../libmyth
LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION
LIBS += -L../.. -lmythmetadata-$$LIBVERSION

contains(QMAKE_CXX, "g++") {
  QMAKE_CXXFLAGS += -O0 -fprofile-arcs -ftest-coverage
  QMAKE_LFLAGS += -fprofile-arcs
}

contains(CONFIG_MYTHLOGSERVER, "yes") {
  LIBS += -L../../../../external/zeromq/src/.libs -lmythzmq
  LIBS += -L../../../../external/nzmqt/src -lmythnzmqt
  QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/zeromq/src/.libs/
  QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/nzmqt/src/
}

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/qjson/lib/
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavutil
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswscale
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavformat
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavcodec
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswresample
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/libhdhomerun
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmyth
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythui
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythupnp
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythservicecontracts
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythtv
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythfreemheg
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../..

# Input
HEADERS += test_videometadata.h
SOURCES += test_videometadata.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; rm -f *.gcov *.gcda *.gcno

LIBS += $$EXTRA_LIBS $$LATE_LIBS
