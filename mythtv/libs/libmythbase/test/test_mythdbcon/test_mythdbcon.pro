include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += network sql testlib

TEMPLATE = app
TARGET = test_mythdbcon
DEPENDPATH += . ../..
INCLUDEPATH += . ../.. ../../..

# Add all the necessary libraries
LIBS += -L../.. -lmythbase-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../..

# Input
HEADERS += test_mythdbcon.h
SOURCES += test_mythdbcon.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
