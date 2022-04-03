include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += testlib

TEMPLATE = app
TARGET = test_bitreader
INCLUDEPATH += ../../..
#LIBS += -L../.. -lmythtv-$$LIBVERSION
#LIBS += -Wl,$$_RPATH_$${PWD}/../..

# Input
HEADERS += test_bitreader.h
SOURCES += test_bitreader.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

#LIBS += $$EXTRA_LIBS $$LATE_LIBS
