include ( ../../../../settings.pro )
include ( ../../../../test.pro )

QT += xml sql network testlib
using_opengl: QT += opengl

TEMPLATE = app
TARGET = test_programinfo
DEPENDPATH += . ../.. ../../audio ../../logging ../../../libmythbase
INCLUDEPATH += . ../.. ../../audio ../../../.. ../../../../external/FFmpeg
 INCLUDEPATH += ../../logging ../../../libmythbase
INCLUDEPATH += ../../../libmythservicecontracts

LIBS += -L../../../libmythbase -lmythbase-$$LIBVERSION
LIBS += -L../../../libmythui -lmythui-$$LIBVERSION
LIBS += -L../../../libmythupnp -lmythupnp-$$LIBVERSION
LIBS += -L../../../libmythservicecontracts -lmythservicecontracts-$$LIBVERSION
LIBS += -L../../../../external/FFmpeg/libavcodec -lmythavcodec
LIBS += -L../../../../external/FFmpeg/libavformat -lmythavformat
LIBS += -L../../../../external/FFmpeg/libavutil -lmythavutil
LIBS += -L../../../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../.. -lmyth-$$LIBVERSION

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavcodec
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavformat
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libavutil
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../../external/FFmpeg/libswresample
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythbase
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythui
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythupnp
QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../../../libmythservicecontracts

# Input
HEADERS += test_programinfo.h
SOURCES += test_programinfo.cpp

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)
QMAKE_CLEAN += ; ( cd $(OBJECTS_DIR) && rm -f *.gcov *.gcda *.gcno )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

# Fix runtime linking on Ubuntu 17.10.
linux:QMAKE_LFLAGS += -Wl,--disable-new-dtags
