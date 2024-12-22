include ( ../../../../settings.pro )

QT += sql testlib widgets
using_opengl: QT += opengl

TEMPLATE = app
TARGET = test_settings
INCLUDEPATH += ../../..

LIBS += -L../.. -lmythui-$$LIBVERSION
LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../..
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase

# Input
HEADERS += test_settings.h
SOURCES += test_settings.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking on Ubuntu 17.10.
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
