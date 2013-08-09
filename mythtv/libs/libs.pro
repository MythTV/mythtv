include (../settings.pro)

TEMPLATE = subdirs

# Libraries without dependencies
SUBDIRS += libmythsoundtouch libmythdvdnav
SUBDIRS += libmythfreesurround libmythbase
SUBDIRS += libmythservicecontracts

using_mheg:SUBDIRS += libmythfreemheg
using_x11:SUBDIRS += libmythnvctrl
!contains( CONFIG_LIBMPEG2EXTERNAL, yes):SUBDIRS += libmythmpeg2

# Libraries with dependencies
SUBDIRS += libmythui libmythupnp libmyth

libmythui.depends = libmythbase
libmythupnp.depends = libmythbase
libmyth.depends =  libmythbase libmythui libmythupnp
libmyth.depends += libmythsoundtouch
libmyth.depends += libmythfreesurround
libmythupnp.depends = libmythbase libmythservicecontracts

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

# unit tests libmythtv
libmythtv-test.depends = sub-libmythtv
libmythtv-test.target = buildtestmythtv
libmythtv-test.commands = cd libmythtv/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += libmythtv-test

unittest.depends = libmyth-test libmythbase-test libmythtv-test
unittest.target = test
unittest.commands = ../programs/scripts/unittests.sh
unix:QMAKE_EXTRA_TARGETS += unittest
