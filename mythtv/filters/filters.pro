include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += invert linearblend denoise3d quickdnr kerneldeint crop force
SUBDIRS += adjust onefield bobdeint ivtc greedyhdeint yadif fieldorder
SUBDIRS += vflip

contains(CONFIG_POSTPROC, yes): SUBDIRS += postprocess

# This filter is currently broken.
# SUBDIRS += convert
