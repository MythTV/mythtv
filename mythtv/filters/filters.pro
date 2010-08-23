include ( ../config.mak )

TEMPLATE = subdirs

# Directories
SUBDIRS += invert linearblend denoise3d quickdnr kerneldeint crop force
SUBDIRS += adjust onefield bobdeint ivtc greedyhdeint yadif fieldorder

contains(CONFIG_POSTPROC, yes): SUBDIRS += postprocess

# This filter is currently broken.
# SUBDIRS += convert
