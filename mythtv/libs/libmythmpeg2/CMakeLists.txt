#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Remove some of the compilation flags that were added to MythTV by
# the -Wextra flag.
string(APPEND CMAKE_C_FLAGS " -Wno-implicit-fallthrough")

#
# Declare the library
#
add_library(
  mythmpeg2
  $<$<BOOL:${HAVE_ALPHA}>:
  motion_comp_alpha.c
  idct_alpha.c
  >
  $<$<BOOL:${HAVE_ALTIVEC}>:
  motion_comp_altivec.c
  idct_altivec.c
  >
  $<$<BOOL:${HAVE_MMX}>:
  idct_mmx.c
  motion_comp_mmx.c
  >
  $<$<BOOL:${HAVE_SPARC}>:
  motion_comp_vis.c
  >
  alloc.c
  cpu_accel.c
  cpu_state.c
  decode.c
  header.c
  idct.c
  motion_comp.c
  slice.c)

#
# All remaining target information
#

# Don't set the hidden visibility flag for this target.
set_property(TARGET mythmpeg2 PROPERTY C_VISIBILITY_PRESET)

target_include_directories(
  mythmpeg2
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} # For config.h
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_link_libraries(mythmpeg2 PUBLIC Threads::Threads)
endif()

#
# Generated files section
#
configure_file(config.h.in config.h)

#
# Installation section
#

install(TARGETS mythmpeg2 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES mpeg2.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythmpeg2)
