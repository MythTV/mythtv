TEMPLATE = subdirs

# Directories
SUBDIRS += invert linearblend denoise3d quickdnr kerneldeint crop force convert
SUBDIRS += adjust onefield bobdeint ivtc greedyhdeint yadif

# This filter is currently broken, because the FFmpeg code that
# it depends on was moved into a seperate library (libpostproc).
# When that library is built by default, this can be re-enabled!
# SUBDIRS += postprocess
