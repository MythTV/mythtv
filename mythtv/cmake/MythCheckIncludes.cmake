#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Include CMake functions
#
include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CMakePushCheckState)

#
# Check for existence of include files
#
check_include_file(altivec.h HAVE_ALTIVEC_H)
check_include_file(byteswap.h HAVE_BYTESWAP_H)
check_include_file(cdio/paranoia.h HAVE_CDIO_PARANOIA_H)
check_include_file(cdio/paranoia/paranoia.h HAVE_CDIO_PARANOIA_PARANOIA_H)
if(ENABLE_D3D)
  check_include_file(d3d9.h HAVE_D3D9_H)
endif()
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(getopt.h HAVE_GETOPT_H)
check_include_file(malloc.h HAVE_MALLOC_H)
check_include_file(mntent.h HAVE_MNTENT_H)
check_include_file(pthread.h HAVE_PTHREAD_H)
check_include_file(soundcard.h HAVE_SOUNDCARD_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(sys/dl.h HAVE_SYS_DL_H)
check_include_file(sys/endian_h HAVE_SYS_ENDIAN_H)
check_include_file(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)

#
# Check for symbols in include files
#
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(close_range "unistd.h" HAVE_CLOSE_RANGE)
cmake_pop_check_state()
check_symbol_exists(fcntl "fcntl.h" HAVE_FCNTL)
check_symbol_exists(getifaddrs "ifaddrs.h" HAVE_GETIFADDRS)
check_symbol_exists(getmntent_r "mntent.h" HAVE_GETMNTENT_R)
check_symbol_exists(getopt "unistd.h" HAVE_GETOPT)
check_symbol_exists(gettimeofday "sys/time.h" HAVE_GETTIMEOFDAY)
check_symbol_exists(posix_fadvise "fcntl.h" HAVE_POSIX_FADVISE)
check_symbol_exists(posix_memalign "stdlib.h" HAVE_POSIX_MEMALIGN)

#
# More complex checks for symbols in include files
#

# udev: fedora:systemd-devel ubuntu:libudev-dev
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES udev)
check_symbol_exists(udev_new "libudev.h" HAVE_LIBUDEV)
cmake_pop_check_state()

if(ENABLE_DXVA2 AND HAVE_D3D9_H)
  check_cxx_source_compiles(
    "
      #include <d3d9.h>
      #include <dxva2api.h>

      DXVA2_ConfigPictureDecode foo;

      int main()
      {
        return 0;
      }
    "
    HAVE_DXVA2)
endif()

check_c_source_compiles(
  "
    #include <arm_neon.h>
    int16x8_t test = vdupq_n_s16(0);" HAVE_INTRINSICS_NEON)
check_c_source_compiles(
  "
  #include <linux/dvb/frontend.h>
  int main(void) {
    return FE_CAN_2G_MODULATION;
  }"
  HAVE_FE_CAN_2G_MODULATION)

#
# Check compiler features
#
check_c_source_compiles(
  "
    int main (int a, char **b)
    { a = __builtin_expect (a, 10); return a == 10 ? 0 : 1; }"
  HAVE_BUILTIN_EXPECT)

#
# Check CPU type
#
check_c_compiler_flag("-mmmx" HAVE_MMX)
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(i?86|x86|amd64|ia64|emt64)")
  set(ARCH_X86 1)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(ppc|powerpc)")
  set(ARCH_PPC 1)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "sparc")
  set(ARCH_SPARC 1)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "alpha")
  set(ARCH_ALPHA 1)
endif()

#
# OS tests
#
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CONFIG_DARWIN 1)
endif()

#
# Misc tests
#
if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  set(HAVE_BIGENDIAN 1)
endif()

#
# Random constant
#
# Argument passed to the FFmpeg av_image_alloc.  A comment in configure says "32
# recommended, 1 used for now"
#
set(IMAGE_ALIGN 1)
