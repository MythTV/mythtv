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
SUBDIRS += libmythui libmyth libmythupnp libmythtv libmythmetadata

libmythui.depends = libmythdb
libmythupnp.depends = libmythdb
libmyth.depends =  libmythdb libmythui libmythupnp
libmyth.depends += libmythsamplerate libmythsoundtouch libmythfreesurround

LIBMYTHTVDEPS = $$SUBDIRS
LIBMYTHTVDEPS -= libmythtv
libmythtv.depends = $$LIBMYTHTVDEPS
libmythmetadata.depends = $$LIBMYTHTVDEPS
