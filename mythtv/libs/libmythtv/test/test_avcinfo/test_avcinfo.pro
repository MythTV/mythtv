include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += testlib

TEMPLATE = app
TARGET = test_avcinfo
INCLUDEPATH += ../../..

# Input
HEADERS += test_avcinfo.h
SOURCES += test_avcinfo.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
