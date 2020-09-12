include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += testlib

TEMPLATE = app
TARGET = test_mythmiscutil
DEPENDPATH += . ../.. ../../logging
INCLUDEPATH += . ../.. ../../logging
LIBS += -L../.. -lmythbase-$$LIBVERSION
LIBS += -Wl,$$_RPATH_$${PWD}/../..

# Input
HEADERS += test_mythmiscutil.h
SOURCES += test_mythmiscutil.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
