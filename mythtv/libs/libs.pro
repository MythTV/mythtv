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
SUBDIRS += lib*/test

unittest.target = test
unittest.commands = ../programs/scripts/unittests.sh
unix:QMAKE_EXTRA_TARGETS += unittest
