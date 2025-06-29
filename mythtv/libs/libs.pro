include (../settings.pro)

TEMPLATE = subdirs

# Libraries without dependencies
SUBDIRS += libmythbase

using_mheg:SUBDIRS += libmythfreemheg
!contains( CONFIG_LIBMPEG2EXTERNAL, yes):SUBDIRS += libmythmpeg2

# Libraries with dependencies
SUBDIRS += libmythui libmythupnp libmyth

libmythui.depends = libmythbase
libmythupnp.depends = libmythbase
libmyth.depends =  libmythbase libmythui libmythupnp

LIBMYTHTVDEPS = $$SUBDIRS

# libmythtv
libmythtv.depends = $$LIBMYTHTVDEPS
SUBDIRS += libmythtv

# libmythmetadata
SUBDIRS += libmythmetadata
libmythmetadata.depends = $$LIBMYTHTVDEPS libmythtv

#libmythmediaserver
SUBDIRS += libmythprotoserver
libmythprotoserver.depends = $$LIBMYTHTVDEPS libmythtv

# unit tests libmyth
libmyth-test.depends = sub-libmyth
libmyth-test.target = buildtestmyth
libmyth-test.commands = cd libmyth/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += libmyth-test

# unit tests libmythbase
libmythbase-test.depends = sub-libmythbase
libmythbase-test.target = buildtestmythbase
libmythbase-test.commands = cd libmythbase/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += libmythbase-test

# unit tests libmythui
libmythui-test.depends = sub-libmythui
libmythui-test.target = buildtestmythui
libmythui-test.commands = cd libmythui/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += libmythui-test

# unit tests libmythtv
libmythtv-test.depends = sub-libmythtv
libmythtv-test.target = buildtestmythtv
libmythtv-test.commands = cd libmythtv/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += libmythtv-test

# unit tests libmythmetadata
libmythmetadata-test.depends = sub-libmythmetadata
libmythmetadata-test.target = buildtestmythmetadata
libmythmetadata-test.commands = cd libmythmetadata/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += libmythmetadata-test

unittest.depends = libmyth-test libmythbase-test libmythui-test libmythtv-test libmythmetadata-test
unittest.target = test
unittest.commands = ../programs/scripts/unittests.sh
unix:QMAKE_EXTRA_TARGETS += unittest

using_mheg {
    # unit tests libmythfreemheg
    libmythfreemheg-test.depends = sub-libmythfreemheg
    libmythfreemheg-test.target = buildtestmythfreemheg
    libmythfreemheg-test.commands = cd libmythfreemheg/test && $(QMAKE) && $(MAKE)
    unix:QMAKE_EXTRA_TARGETS += libmythfreemheg-test
    unittest.depends += libmythfreemheg-test
}
