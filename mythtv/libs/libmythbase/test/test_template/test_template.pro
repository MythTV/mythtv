include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib

TEMPLATE = app
TARGET = test_template
DEPENDPATH += . ../..
INCLUDEPATH += . ../..
LIBS += -L../.. -lmythbase-$$LIBVERSION

# Input
HEADERS += test_template.h
SOURCES += test_template.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
