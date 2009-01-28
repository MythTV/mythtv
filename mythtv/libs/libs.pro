include (../config.mak)
include (../settings.pro)

TEMPLATE = subdirs

# Libraries without dependencies
SUBDIRS += libavutil libavcodec libavformat libmythsamplerate
SUBDIRS += libmythsoundtouch libmythmpeg2 libmythdvdnav
SUBDIRS += libmythfreesurround libmythdb

using_mheg:SUBDIRS += libmythfreemheg
using_live:SUBDIRS += libmythlivemedia
using_hdhomerun:SUBDIRS += libmythhdhomerun

# Libraries with dependencies
SUBDIRS += libmythui libmyth libmythupnp libmythtv
libmythui.depends = libmythdb
libmythupnp.depends = libmythdb
libmyth.depends = libmythdb libmythui libmythupnp

LIBMYTHTVDEPS = $$SUBDIRS
LIBMYTHTVDEPS -= libmythtv
libmythtv.depends = $$LIBMYTHTVDEPS

