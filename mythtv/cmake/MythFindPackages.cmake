#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Include CMake functions
#
include(CMakePrintHelpers)
# include(CMakePrintSystemInformation)

#
# Include MythTV CMake functions
#
include(BuildConfigString)

#
# This module needs pkg-config functionality
#
find_package(PkgConfig REQUIRED)

#
# Packages provided by MythTV.  These have no dependencies on MythTV and must
# have already been compiled and installed by the super-package.
#
if(NOT CMAKE_CROSSCOMPILING
   AND ENABLE_NVDEC
   AND CMAKE_SYSTEM_NAME MATCHES "Linux|Windows"
   AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86|aarch64")
  pkg_check_modules(FFNVCODEC ffnvcodec REQUIRED IMPORTED_TARGET)
  add_build_config(PkgConfig::FFNVCODEC "nvdec")
  if(TARGET PkgConfig::FFNVCODEC)
    # Take our cue from ffmpeg
    find_program(_ffmpeg mythffmpeg)
    find_library(_avcodec mythavdevice)
    cmake_path(GET _avcodec PARENT_PATH _avcodec_path)
    if(NOT EXISTS ${_ffmpeg})
      message(FATAL_ERROR "Cannot find our ffmpeg executable at ${_ffmpeg}")
    endif()
    if(NOT EXISTS ${_avcodec})
      message(FATAL_ERROR "Cannot find our mythavdevice library ${_avcodec}")
    endif()
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${_avcodec_path}
              ${_ffmpeg} -decoders
      RESULT_VARIABLE _result
      OUTPUT_VARIABLE _output
      ERROR_VARIABLE _errors)
    if((NOT _result EQUAL 0) OR ("${_output}" EQUAL ""))
      message(STATUS "mythffmpeg failed. Cannot determine support for NVDEC.")
      message(STATUS "This indicates a problem with your FFmpeg build for MythTV.")
      list(APPEND CMAKE_MESSAGE_INDENT "  ")
      message(STATUS "Command: LD_LIBRARY_PATH=${_avcodec_path} ${_ffmpeg} -decoders")
      message(STATUS "Return status: ${_result}")
      message(STATUS "Normal output:\n${_output}")
      message(STATUS "Error output:\n${_errors}")
      message(FATAL_ERROR "Please fix the problem running mythffmpeg.")
    endif()
    string(FIND ${_output} cuvid CUVID_OFFSET)
    if(NOT CUVID_OFFSET EQUAL -1)
      message(STATUS "FFmpeg supports NVDEC.  Enabling support in MythTV.")
      set(CONFIG_NVDEC TRUE)
    endif()
  endif()
endif()

#
# Packages where MythTV has a replacement if there is no system version.  These
# have no dependencies on MythTV and must have already been compiled and
# installed by the super-package.
#
pkg_check_modules(LIBUDFREAD "libudfread>=1.1.1" REQUIRED IMPORTED_TARGET)

#
# Find an exiv2 that has c++17 support.  The current version number is 0.27 and
# the c++17 based version is supposed to be 1.0 but is included in 0.28
#
pkg_check_modules(EXIV2 "exiv2>=0.28" QUIET IMPORTED_TARGET)
if(NOT EXIV2_FOUND)
  pkg_check_modules(EXIV2 "mythexiv2>=0.28" REQUIRED IMPORTED_TARGET)
endif()

#
# If not provided by the system, this is currently built as part of mythtv (not
# the super-package) because it does have some tweaks.
#
pkg_check_modules(SYSTEM_LIBBLURAY "libbluray>=0.9.3" IMPORTED_TARGET)
if(SYSTEM_LIBBLURAY_FOUND)
  # Create alias, so that all the rest of the CMakeLists files just refer to
  # mythbluray.
  add_library(mythbluray ALIAS PkgConfig::SYSTEM_LIBBLURAY)
  add_build_config(PkgConfig::SYSTEM_LIBBLURAY "system_libbluray")
else()
  message(STATUS "  Will build MythTV copy of libbluray.")
  add_build_config(ON "bluray")
endif()

#
# Enable optional MythTV components
#

if(ENABLE_HDHOMERUN)
  find_package(HDHomerun 20190625)
  add_build_config(HDHomerun::HDHomerun "hdhomerun")
endif()
if(ENABLE_VBOX)
  message(STATUS "Adding vbox support")
  add_library(mythtv_vbox INTERFACE)
  add_build_config(TRUE "vbox")
  set(CONFIG_VBOX TRUE)
endif()
if(ENABLE_CETON)
  message(STATUS "Adding ceton support")
  add_library(mythtv_ceton INTERFACE)
  add_build_config(TRUE "ceton")
  set(CONFIG_CETON TRUE)
endif()

if(ENABLE_SATIP)
  message(STATUS "Adding satip support")
  add_library(mythtv_satip INTERFACE)
  add_build_config(TRUE "satip")
  set(CONFIG_SATIP TRUE)
endif()
if(ENABLE_DVB)
  if(NOT APPLE AND NOT WIN32)
    find_file(DVB_FRONTEND_H "linux/dvb/frontend.h")
    if(DVB_FRONTEND_H)
      message(STATUS "Adding dvb support (method 1)")
      add_library(mythtv_dvb INTERFACE)
      add_build_config(TRUE "dvb")
      set(CONFIG_DVB TRUE)
    elseif(MYTH_DVB_PATH)
      find_file(DVB_FRONTEND_H "linux/dvb/frontend.h" PATHS "${MYTH_DVB_PATH}")
      if(DVB_FRONTEND_H)
        message(STATUS "Adding dvb support (method 2)")
        add_library(mythtv_dvb INTERFACE)
        add_build_config(TRUE "dvb")
        set(CONFIG_DVB TRUE)
        target_include_directories(mythtv_dvb INTERFACE "${MYTH_DVB_PATH}")
      else()
        message(STATUS "dvb support not found")
      endif()
    endif()
  endif()
endif()

# Linear Systems Ltd.'s Master Linux SDK
# https://github.com/kierank/dveo-linux-master
if(ENABLE_ASI)
  find_file(_ASI_H dveo/asi.h)
  find_file(_MASTER_H dveo/master.h)
  if(_ASI_H AND _MASTER_H)
    add_library(mythtv_asi INTERFACE)
    add_build_config(TRUE "dvb")
    set(CONFIG_ASI TRUE)
  endif()
endif()

if(ENABLE_JOYSTICK_MENU)
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(STATUS "Checking for library 'udev'")
    find_library(LINUX_UDEV NAMES udev libudev libudev.so libudev.so.1)
    if(LINUX_UDEV)
      message(STATUS "  Found ${LINUX_UDEV}")
      set(CONFIG_JOYSTICK_MENU TRUE)
      add_library(joystick INTERFACE)
      add_build_config(TRUE "joystick_menu")
      target_link_libraries(joystick INTERFACE ${LINUX_UDEV})
    else()
      message(STATUS "  Can't find the udev library. Disabling joystick.")
    endif()
  endif()
endif()

if(ENABLE_LIRC AND NOT WIN32)
  # This uses the socket interface. No libs necessary
  message(STATUS "Adding lirc support")
  add_library(lirc INTERFACE)
  add_build_config(TRUE "lirc")
  set(CONFIG_LIRC TRUE)
endif()

if(ENABLE_MHEG)
  message(STATUS "Adding mheg support")
  add_library(mythtv_mheg INTERFACE)
  add_build_config(TRUE "mheg")
  set(CONFIG_MHEG TRUE)
endif()

if(APPLE)
  find_library(APPLE_APPLICATIONSERVICES_LIBRARY ApplicationServices)
  find_library(APPLE_AUDIOTOOLBOX_LIBRARY AudioToolbox)
  find_library(APPLE_AUDIOUNIT_LIBRARY AudioUnit)
  find_library(APPLE_AVCVIDEOSERVICES_LIBRARY AVCVideoServices
               PATHS ${MYTH_FIREWIRE_SDK})
  find_library(APPLE_COCOA_LIBRARY Cocoa)
  find_library(APPLE_COREAUDIO_LIBRARY CoreAudio)
  find_library(APPLE_COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(APPLE_CORESERVICES_LIBRARY CoreServices)
  find_library(APPLE_COREVIDEO_LIBRARY CoreVideo)
  find_library(APPLE_DISKARBITRATION_LIBRARY DiskArbitration)
  find_library(APPLE_IOKIT_LIBRARY IOKit)
  find_library(APPLE_IOSURFACE_LIBRARY IOSurface)
  find_library(APPLE_OPENGL_LIBRARY OpenGL)
  find_library(APPLE_VIDEOTOOLBOX_LIBRARY VideoToolbox)
  if(APPLE_AVCVIDEOSERVICES_LIBRARY)
    cmake_path(GET APPLE_AVCVIDEOSERVICES_LIBRARY PARENT_PATH
               APPLE_AVCVIDEOSERVICES_HEADERS)
  else()
    message(STATUS "Firewire being disabled. FireWire SDK missing.")
    set(ENABLE_FIREWIRE OFF)
  endif()

  message(VERBOSE "Frameworks found:")
  message(VERBOSE "  APPLE_APPLICATIONSERVICES_LIBRARY: ${APPLE_APPLICATIONSERVICES_LIBRARY}")
  message(VERBOSE "  APPLE_AUDIOTOOLBOX_LIBRARY: ${APPLE_AUDIOTOOLBOX_LIBRARY}")
  message(VERBOSE "  APPLE_AUDIOUNIT_LIBRARY: ${APPLE_AUDIOUNIT_LIBRARY}")
  message(VERBOSE "  APPLE_AVCVIDEOSERVICES_LIBRARY: ${APPLE_AVCVIDEOSERVICES_LIBRARY}")
  message(VERBOSE "  APPLE_COCOA_LIBRARY: ${APPLE_COCOA_LIBRARY}")
  message(VERBOSE "  APPLE_COREAUDIO_LIBRARY: ${APPLE_COREAUDIO_LIBRARY}")
  message(VERBOSE "  APPLE_COREFOUNDATION_LIBRARY: ${APPLE_COREFOUNDATION_LIBRARY}")
  message(VERBOSE "  APPLE_CORESERVICES_LIBRARY: ${APPLE_CORESERVICES_LIBRARY}")
  message(VERBOSE "  APPLE_COREVIDEO_LIBRARY: ${APPLE_COREVIDEO_LIBRARY}")
  message(VERBOSE "  APPLE_DISKARBITRATION_LIBRARY: ${APPLE_DISKARBITRATION_LIBRARY}")
  message(VERBOSE "  APPLE_IOKIT_LIBRARY: ${APPLE_IOKIT_LIBRARY}")
  message(VERBOSE "  APPLE_IOSURFACE_LIBRARY: ${APPLE_IOSURFACE_LIBRARY}")
  message(VERBOSE "  APPLE_OPENGL_LIBRARY: ${APPLE_OPENGL_LIBRARY}")
  message(VERBOSE "  APPLE_VIDEOTOOLBOX_LIBRARY: ${APPLE_VIDEOTOOLBOX_LIBRARY}")

  #
  # Check for symbols in darwin include files
  #
  cmake_push_check_state()
  set(CMAKE_REQUIRED_LIBRARIES ${APPLE_IOKIT_LIBRARY})
  check_symbol_exists(IOMainPort "IOKit/IOKitLib.h" HAVE_IOMAINPORT)
  cmake_pop_check_state()

  # If ffmpeg disabled videotoolbox then disable it
  find_program(_ffmpeg mythffmpeg)
  find_library(_avcodec mythavdevice)
  cmake_path(GET _avcodec PARENT_PATH _avcodec_path)
  if(NOT EXISTS ${_ffmpeg})
    message(FATAL_ERROR "Cannot find our ffmpeg executable at ${_ffmpeg}")
  endif()
  if(NOT EXISTS ${_avcodec})
    message(FATAL_ERROR "Cannot find our mythavdevice library ${_avcodec}")
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${_avcodec_path}
            ${_ffmpeg} -encoders
    OUTPUT_VARIABLE _output
    ERROR_QUIET)
  message(VERBOSE "ffmpeg -encoders output: ${_output}")
  string(FIND ${_output} videotoolbox VIDEOTOOLBOX_OFFSET)
  if(VIDEOTOOLBOX_OFFSET EQUAL -1)
    message(STATUS "disabling videotoolbox support (not present in ffmpeg)")
    unset(APPLE_VIDEOTOOLBOX_LIBRARY)
  endif()

  # Set config variable based on library presence
  set(CONFIG_DARWIN_DA ${APPLE_DISKARBITRATION_LIBRARY})
  set(CONFIG_VIDEOTOOLBOX ${APPLE_VIDEOTOOLBOX_LIBRARY})

  mark_as_advanced(
    APPLE_APPLICATIONSERVICES_LIBRARY
    APPLE_AUDIOTOOLBOX_LIBRARY
    APPLE_AUDIOUNIT_LIBRARY
    APPLE_COCOA_LIBRARY
    APPLE_COREAUDIO_LIBRARY
    APPLE_COREFOUNDATION_LIBRARY
    APPLE_CORESERVICES_LIBRARY
    APPLE_COREVIDEO_LIBRARY
    APPLE_DISKARBITRATION_LIBRARY
    APPLE_IOKIT_LIBRARY
    APPLE_IOSURFACE_LIBRARY
    APPLE_OPENGL_LIBRARY
    APPLE_VIDEOTOOLBOX_LIBRARY)
endif()

if(ENABLE_FIREWIRE)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    if(APPLE_AVCVIDEOSERVICES_LIBRARY)
      message(STATUS "Adding apple firewire support")
      add_library(firewire INTERFACE)
      target_link_libraries(firewire INTERFACE ${APPLE_AVCVIDEOSERVICES_LIBRARY})
      target_compile_options(firewire INTERFACE -iframework ${APPLE_AVCVIDEOSERVICES_HEADERS})
      set(CONFIG_FIREWIRE TRUE)
      set(CONFIG_FIREWIRE_OSX TRUE)
    endif()
  else()
    # ~~~
    # libiec61883: fedora:libiec61883-devel debian:libiec61883-dev
    # libavc1394: fedora:libavc1394-devel debian:libavc1394-dev
    # ~~~
    pkg_check_modules(LIBAVC1394 libavc1394 IMPORTED_TARGET)
    pkg_check_modules(LIBIEC61883 libiec61883 IMPORTED_TARGET)
    if(TARGET PkgConfig::LIBAVC1394 AND TARGET PkgConfig::LIBIEC61883)
      message(STATUS "Adding linux firewire support")
      add_library(firewire INTERFACE)
      target_link_libraries(firewire INTERFACE
        PkgConfig::LIBIEC61883
        PkgConfig::LIBAVC1394)
      set(CONFIG_FIREWIRE TRUE)
      set(CONFIG_FIREWIRE_LINUX TRUE)
    endif()
  endif()
endif()
add_build_config(firewire "firewire")
