include ( ../../../../settings.pro )

QT += testlib

TEMPLATE = app
TARGET = test_avcinfo
DEPENDPATH += . ../..
INCLUDEPATH += . ../.. ../../mpeg ../../../libmythui ../../../libmyth ../../../libmythbase

contains(QMAKE_CXX, "g++") {
  QMAKE_CXXFLAGS += -O0 -fprofile-arcs -ftest-coverage
  QMAKE_LFLAGS += -fprofile-arcs
}

# Input
HEADERS += test_avcinfo.h
SOURCES += test_avcinfo.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
