include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib

TEMPLATE = app
TARGET = test_mythcommandlineparser
DEPENDPATH += . ../..
INCLUDEPATH += . ../..
LIBS += -L../.. -lmythbase-$$LIBVERSION
LIBS += -Wl,$$_RPATH_$${PWD}/../..

# Input
HEADERS += test_mythcommandlineparser.h
SOURCES += test_mythcommandlineparser.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
