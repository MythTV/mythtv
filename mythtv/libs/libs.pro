include (../settings.pro)

TEMPLATE = subdirs

# Libraries without dependencies
SUBDIRS += libmythsamplerate
SUBDIRS += libmythsoundtouch libmythdvdnav
SUBDIRS += libmythbluray libmythfreesurround libmythdb

using_mheg:SUBDIRS += libmythfreemheg
using_live:SUBDIRS += libmythlivemedia
using_hdhomerun:SUBDIRS += libmythhdhomerun
using_x11:SUBDIRS += libmythnvctrl
!contains( CONFIG_LIBMPEG2EXTERNAL, yes):SUBDIRS += libmythmpeg2

# Libraries with dependencies
SUBDIRS += libmythui libmythupnp libmyth

libmythui.depends = libmythdb
libmythupnp.depends = libmythdb
libmyth.depends =  libmythdb libmythui libmythupnp
libmyth.depends += libmythsamplerate libmythsoundtouch libmythfreesurround

LIBMYTHTVDEPS = $$SUBDIRS

# libmythtv
libmythtv.depends = $$LIBMYTHTVDEPS
SUBDIRS += libmythtv

# libmythmetadata
SUBDIRS += libmythmetadata
libmythmetadata.depends = $$LIBMYTHTVDEPS libmythtv
