#
# Copyright (C) 2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Ensure that we find the MythTV installed version of FFmpeg before finding the
# system installed version of FFmpeg.
include_directories(BEFORE ${LIBS_INSTALL_PREFIX}/include/mythtv)
