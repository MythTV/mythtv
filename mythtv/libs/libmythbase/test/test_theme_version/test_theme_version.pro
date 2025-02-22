include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += testlib

TEMPLATE = app
TARGET = test_theme_version
DEPENDPATH += . ../.. ../../..
INCLUDEPATH += . ../.. ../../..
LIBS += -L../.. -lmythbase-$$LIBVERSION
LIBS += -Wl,$$_RPATH_$${PWD}/../..

DEFINES += TEST_SOURCE_DIR='\'"$${PWD}"\''

# Input
HEADERS += test_theme_version.h
SOURCES += test_theme_version.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
