include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += network sql widgets xml testlib

TEMPLATE = app
TARGET = test_mheg_parser
DEPENDPATH += . ../..
INCLUDEPATH += . ../..
INCLUDEPATH += ../../../../libs

# Add all the necessary libraries
LIBS += -L../../../../libs/libmythbase -lmythbase-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../libs/libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../

DEFINES += TEST_SOURCE_DIR='\'"$${PWD}"'\'

# Input
HEADERS += test_mheg_parser.h
SOURCES += test_mheg_parser.cpp
SOURCES += ../../Actions.cpp
SOURCES += ../../BaseActions.cpp
SOURCES += ../../BaseClasses.cpp
SOURCES += ../../Bitmap.cpp
SOURCES += ../../Engine.cpp
SOURCES += ../../Groups.cpp
SOURCES += ../../Ingredients.cpp
SOURCES += ../../Link.cpp
SOURCES += ../../ParseBinary.cpp
SOURCES += ../../ParseNode.cpp
SOURCES += ../../ParseText.cpp
SOURCES += ../../Programs.cpp
SOURCES += ../../Root.cpp
SOURCES += ../../Text.cpp
SOURCES += ../../Variables.cpp
SOURCES += ../../Visible.cpp
SOURCES += ../../DynamicLineArt.cpp
SOURCES += ../../TokenGroup.cpp
SOURCES += ../../Stream.cpp
SOURCES += ../../Presentable.cpp

QMAKE_CLEAN += $(TARGET)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
