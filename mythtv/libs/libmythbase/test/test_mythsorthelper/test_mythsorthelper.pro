include ( ../../../../settings.pro )

QT += xml sql network testlib

TEMPLATE = app
TARGET = test_mythsorthelper
DEPENDPATH += . ../.. ../../logging
INCLUDEPATH += . ../.. ../../logging
LIBS += -L../.. -lmythbase-$$LIBVERSION
LIBS += -Wl,$$_RPATH_$${PWD}/../..

contains(QMAKE_CXX, "g++") {
  QMAKE_CXXFLAGS += -O0 -fprofile-arcs -ftest-coverage
  QMAKE_LFLAGS += -fprofile-arcs
}

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../..

# Input
HEADERS += test_mythsorthelper.h
SOURCES += test_mythsorthelper.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
