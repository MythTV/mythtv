#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# The following directories are included from the top level CMakeLists.txt file
# as External Projects:
#
# ~~~
# FFmpeg           (no dependancies)
# libexiv2         (no dependancies or customizations)
# libudfread       (no dependancies or customizations)
# nv-codec-headers (no dependancies or customizations)
# ~~~

add_subdirectory(libmythdvdnav)
if(NOT SYSTEM_LIBBLURAY_FOUND)
  add_subdirectory(libmythbluray)
endif()
