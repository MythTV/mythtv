include (../config.mak)
include (../settings.pro)

TEMPLATE = subdirs
CONFIG += ordered

# Directories
SUBDIRS += libavutil libavcodec libavformat libmythsamplerate 
SUBDIRS += libmythsoundtouch libmythmpeg2 libmythdvdnav
SUBDIRS += libmythfreesurround
SUBDIRS += libmythdb libmythupnp libmythui libmyth
using_mheg:SUBDIRS += libmythfreemheg
using_live:SUBDIRS += libmythlivemedia

# Libmythtv must be last because it links against everything
SUBDIRS += libmythtv
