include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += testlib

TEMPLATE = app
TARGET = test_mythdate
DEPENDPATH += . ../..
INCLUDEPATH += . ../..
LIBS += -L../.. -lmythbase-$$LIBVERSION
LIBS += -Wl,$$_RPATH_$${PWD}/../..

# Input
HEADERS += test_mythdate.h
SOURCES += test_mythdate.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
