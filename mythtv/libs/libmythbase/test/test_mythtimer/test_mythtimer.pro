include ( ../../../../settings.pro )

contains(QT_VERSION, ^4\\.[0-9]\\..*) {
CONFIG += qtestlib
}
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += testlib
}

TEMPLATE = app
TARGET = test_mythtimer
DEPENDPATH += . ../..
INCLUDEPATH += . ../..

contains(QMAKE_CXX, "g++") {
  QMAKE_CXXFLAGS += -O0 -fprofile-arcs -ftest-coverage 
  QMAKE_LFLAGS += -fprofile-arcs 
}
QMAKE_LFLAGS += -Wl,-rpath=$(PWD)/../../../../external/zeromq/src/.libs/
QMAKE_LFLAGS += -Wl,-rpath=$(PWD)/../../../../external/nzmqt/src/
QMAKE_LFLAGS += -Wl,-rpath=$(PWD)/../../../../external/qjson/lib/

# Input
HEADERS += test_mythtimer.h
SOURCES += test_mythtimer.cpp

HEADERS += ../../mythtimer.h
SOURCES += ../../mythtimer.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; rm -f *.gcov *.gcda *.gcno

LIBS += $$EXTRA_LIBS $$LATE_LIBS
