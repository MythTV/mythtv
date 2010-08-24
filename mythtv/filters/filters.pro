include ( ../config.mak )

TEMPLATE = subdirs

# Directories
SUBDIRS += invert linearblend denoise3d quickdnr kerneldeint crop force
SUBDIRS += adjust onefield bobdeint ivtc greedyhdeint yadif fieldorder

contains(CONFIG_POSTPROC, yes): SUBDIRS += postprocess

# This filter is currently broken.
# SUBDIRS += convert

# If MMX is enabled, 10.6 cannot build this (for now)
macx:contains(HAVE_MMX, yes): SUBDIRS -= yadif
