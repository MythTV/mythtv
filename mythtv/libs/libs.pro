include (../settings.pro)

TEMPLATE = subdirs
CONFIG += ordered

# Directories
SUBDIRS += libavutil libavcodec libavformat libmythsamplerate 
SUBDIRS += libmythsoundtouch libmythmpeg2 libmythdvdnav
SUBDIRS += libmythfreesurround
SUBDIRS += libmythdb libmythupnp libmythui libmyth
SUBDIRS += libmythfreemheg libmythlivemedia

# Libmythtv must be last because it links against everything
SUBDIRS += libmythtv
