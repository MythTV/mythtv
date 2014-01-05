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
LIBS += -L../.. -lmythbase-$$LIBVERSION -lmythmetadata-$$LIBVERSION

contains(QMAKE_CXX, "g++") {
  QMAKE_CXXFLAGS += -O0 -fprofile-arcs -ftest-coverage
  QMAKE_LFLAGS += -fprofile-arcs
}

# QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/zeromq/src/.libs/
# QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/nzmqt/src/
# QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/qjson/lib/
# QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../..

# Input
HEADERS += test_videometadata.h
SOURCES += test_videometadata.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; rm -f *.gcov *.gcda *.gcno

LIBS += $$EXTRA_LIBS $$LATE_LIBS
