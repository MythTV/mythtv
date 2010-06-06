include (../settings.pro)

TEMPLATE = subdirs

# Libraries without dependencies
SUBDIRS += libavutil libmythsamplerate
SUBDIRS += libpostproc
SUBDIRS += libmythsoundtouch libmythmpeg2 libmythdvdnav
SUBDIRS += libmythbdnav libmythfreesurround libmythdb

using_mheg:SUBDIRS += libmythfreemheg
using_live:SUBDIRS += libmythlivemedia
using_hdhomerun:SUBDIRS += libmythhdhomerun
using_x11:SUBDIRS += libmythnvctrl

# Libraries with dependencies
SUBDIRS += libmythui libmyth libmythupnp libmythtv libavcodec libavformat libswscale
libmythui.depends = libmythdb
libmythupnp.depends = libmythdb
libmyth.depends = libavcodec libmythdb libmythui libmythupnp
libmyth.depends += libmythsamplerate libmythsoundtouch libmythfreesurround

libavcodec.depends = libavutil
libavformat.depends = libavcodec libavutil
libswscale.depends = libavutil

LIBMYTHTVDEPS = $$SUBDIRS
LIBMYTHTVDEPS -= libmythtv
libmythtv.depends = $$LIBMYTHTVDEPS

