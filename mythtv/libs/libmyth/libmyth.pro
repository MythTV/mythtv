include ( ../../settings.pro )

QT += network xml sql widgets
android: QT += androidextras

TEMPLATE = lib
TARGET = myth-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target
DEFINES += MYTH_API

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

POSTINC =

contains(INCLUDEPATH, /usr/include) {
  POSTINC += /usr/include
  INCLUDEPATH -= /usr/include
}
contains(INCLUDEPATH, /usr/local/include) {
  POSTINC += /usr/local/include
  INCLUDEPATH -= /usr/local/include
}
contains(INCLUDEPATH, /usr/X11R6/include) {
  POSTINC += /usr/X11R6/include
  INCLUDEPATH -= /usr/X11R6/include
}

# Input
HEADERS += backendselect.h
HEADERS += guistartup.h
HEADERS += mythcontext.h
HEADERS += mythexp.h

SOURCES += backendselect.cpp
SOURCES += guistartup.cpp
SOURCES += mythcontext.cpp

INCLUDEPATH += ..
INCLUDEPATH += $${POSTINC}

LIBS += -L../libmythbase           -lmythbase-$${LIBVERSION}
LIBS += -L../libmythui           -lmythui-$${LIBVERSION}
LIBS += -L../libmythupnp         -lmythupnp-$${LIBVERSION}

# Install headers so that plugins can compile independently
inc.path = $${PREFIX}/include/mythtv/libmyth
inc.files  = mythcontext.h
inc.files += mythexp.h

INSTALLS += inc

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

DISTFILES += \
    Makefile

test_clean.commands = -cd test/ && $(MAKE) -f Makefile clean
clean.depends = test_clean
QMAKE_EXTRA_TARGETS += test_clean clean
test_distclean.commands = -cd test/ && $(MAKE) -f Makefile distclean
distclean.depends = test_distclean
QMAKE_EXTRA_TARGETS += test_distclean distclean
