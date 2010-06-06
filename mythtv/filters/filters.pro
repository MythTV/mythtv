TEMPLATE = subdirs

# Directories
SUBDIRS += invert linearblend denoise3d quickdnr kerneldeint crop force
SUBDIRS += adjust onefield bobdeint ivtc greedyhdeint yadif fieldorder
SUBDIRS += postprocess

# This filter is currently broken.
# SUBDIRS += convert
