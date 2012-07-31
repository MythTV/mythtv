include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += invert linearblend denoise3d quickdnr kerneldeint crop force
SUBDIRS += adjust onefield bobdeint ivtc greedyhdeint yadif fieldorder
SUBDIRS += vflip

contains(CONFIG_POSTPROC, yes): SUBDIRS += postprocess

# greeydy doesn't compile on mac 32 bits
macx:!contains( QMAKE_CXXFLAGS, "x86_64" ) {
    SUBDIRS -= greedyhdeint
}

# This filter is currently broken.
# SUBDIRS += convert
