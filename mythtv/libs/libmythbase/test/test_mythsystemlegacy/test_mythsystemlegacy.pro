include ( ../../../../settings.pro )

QT += xml sql network testlib

TEMPLATE = app
TARGET = test_mythsystemlegacy
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
HEADERS += test_mythsystemlegacy.h
SOURCES += test_mythsystemlegacy.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
