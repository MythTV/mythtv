#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Look for precompiled libraries. Debian doesn't have many of these, but Fedora
# has a bunch.
set(MINGW_SYSROOT "/usr/${TOOLCHAIN_PREFIX}/sys-root/mingw")
list(PREPEND PKG_CONFIG_PATH "${MINGW_SYSROOT}/lib/pkgconfig")
list(APPEND CMAKE_FIND_ROOT_PATH "${MINGW_SYSROOT}/lib/pkgconfig")
if(EXISTS ${MINGW_SYSROOT}/lib/libz.dll.a)
  set(ZLIB_ROOT ${MINGW_SYSROOT})
else()
  set(ZLIB_ROOT ${LIBS_INSTALL_PREFIX})
endif()
