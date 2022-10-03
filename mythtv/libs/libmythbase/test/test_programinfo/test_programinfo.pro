include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib
using_opengl: QT += opengl

TEMPLATE = app
TARGET = test_programinfo
INCLUDEPATH += ../../..
LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase

# Input
HEADERS += test_programinfo.h
SOURCES += test_programinfo.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking on Ubuntu 17.10.
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
