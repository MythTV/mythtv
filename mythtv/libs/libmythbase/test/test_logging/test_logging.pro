include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib

TEMPLATE = app
TARGET = test_logging
DEPENDPATH += . ../..
INCLUDEPATH += . ../..
LIBS += -L../.. -lmythbase-$$LIBVERSION
LIBS += -Wl,$$_RPATH_$${PWD}/../..

# Input
HEADERS += test_logging.h
SOURCES += test_logging.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
