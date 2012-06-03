include ( ../config.mak )
include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += invert linearblend denoise3d quickdnr kerneldeint crop force
SUBDIRS += adjust onefield bobdeint ivtc yadif fieldorder

contains(CONFIG_POSTPROC, yes): SUBDIRS += postprocess

# greeydy doesn't compile on 32 bits
contains( QMAKE_CXXFLAGS, "x86_64" ) {
    SUBDIRS += greedyhdeint
}

# This filter is currently broken.
# SUBDIRS += convert

# If MMX is enabled, 10.6 cannot build this (for now)
macx:contains(HAVE_MMX, yes): SUBDIRS -= yadif
