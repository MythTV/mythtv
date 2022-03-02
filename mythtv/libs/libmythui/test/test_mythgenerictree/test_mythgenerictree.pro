include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += widgets testlib

TEMPLATE = app
TARGET = test_mythgenerictree
INCLUDEPATH += ../../..

# Add all the necessary libraries
LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION
LIBS += -L../.. -lmythui-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../

# Input
HEADERS += test_mythgenerictree.h
SOURCES += test_mythgenerictree.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
