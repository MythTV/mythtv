include (../settings.pro)

TEMPLATE = subdirs

# Libraries without dependencies
SUBDIRS += libmythsamplerate
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
libmyth.depends += libmythsamplerate libmythsoundtouch libmythfreesurround
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

# unit tests
libmythbase-test.depends = sub-libmythbase
libmythbase-test.target = buildtest
libmythbase-test.commands = cd libmythbase/test && $(QMAKE) && $(MAKE)
unix:QMAKE_EXTRA_TARGETS += libmythbase-test

unittest.depends = libmythbase-test
unittest.target = test
unittest.commands = ../programs/scripts/unittests.sh
unix:QMAKE_EXTRA_TARGETS += unittest
