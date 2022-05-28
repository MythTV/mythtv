include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib widgets
using_opengl: QT += opengl

TEMPLATE = app
TARGET = test_rssparse
DEPENDPATH += ../..
INCLUDEPATH += ../../..

# Add all the necessary libraries
LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase

# Input
HEADERS += test_rssparse.h
SOURCES += test_rssparse.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
