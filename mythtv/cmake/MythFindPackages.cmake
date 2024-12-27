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
      OUTPUT_VARIABLE _output
      ERROR_QUIET)
    string(FIND ${_output} cuvid CUVID_OFFSET)
    if(NOT CUVID_OFFSET EQUAL -1)
      target_compile_definitions(PkgConfig::FFNVCODEC INTERFACE USING_NVDEC)
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
  target_compile_definitions(PkgConfig::SYSTEM_LIBBLURAY
                             INTERFACE HAVE_LIBBLURAY)
  # Create alias, so that all the rest of the CMakeLists files just refer to
  # mythbluray.
  add_library(mythbluray ALIAS PkgConfig::SYSTEM_LIBBLURAY)
  add_build_config(PkgConfig::SYSTEM_LIBBLURAY "system_libbluray")
else()
  message(STATUS "  Will build MythTV copy of libbluray.")
  add_build_config(ON "bluray")
endif()

#
# System packages
#
find_package(Iconv REQUIRED)
find_package(expat NO_MODULE)
find_package(ZLIB REQUIRED)
find_package(SQLite3 3.13)
add_build_config(SQLite::SQLite3 "sqlite3")
set(CONFIG_SQLITE3 ${SQLite3_FOUND})

#
# Bindings
#
if(ENABLE_BINDINGS_PYTHON)
  find_package(
    Python3 3.6
    COMPONENTS Interpreter
    REQUIRED)
  if(TARGET Python3::Interpreter)
    find_package(
      Python3Modules
      OPTIONAL_COMPONENTS
        MySQLdb
        lxml
        pip>=23.0.1
        requests
        wheel)
  endif()

  if(NOT TARGET Python3::Interpreter)
    message(STATUS "Missing python interpreter. Disabling python bindings.")
  elseif(Python3Modules_REQUIRED_MISSING)
    message(STATUS "Missing some python modules. Disabling python bindings.")
  else()
    message(
      STATUS "Python interperter and modules found. Enbling python bindings.")
    set(USING_BINDINGS_PYTHON TRUE)
    set(CONFIG_BINDINGS_PYTHON TRUE)
  endif()

  # Strip off any minor version on the python executable name.
  if(Python3_EXECUTABLE MATCHES "(.*)\\.[0-9]+")
    set(Python3_EXECUTABLE ${CMAKE_MATCH_1})
  endif()
  add_build_config(USING_BINDINGS_PYTHON "bindings_python")
endif()

if(ENABLE_BINDINGS_PERL)
  find_package(Perl)
  if(PERL_FOUND)
    find_package(
      PerlModules
      OPTIONAL_COMPONENTS
        Config
        DBD::mysql
        DBI
        Exporter
        ExtUtils::MakeMaker
        Fcntl
        File::Copy
        HTTP::Request
        IO::Socket::INET6
        LWP::UserAgent
        Net::UPnP::ControlPoint
        Net::UPnP::QueryResponse
        Sys::Hostname
        XML::Simple)
  endif()
  if(NOT PERL_FOUND)
    message(STATUS "Missing perl interpreter. Disabling perl bindings.")
  elseif(PerlModules_REQUIRED_MISSING)
    message(STATUS "Missing some perl modules. Disabling perl bindings.")
  else()
    message(STATUS "Perl interperter and modules found. Enbling perl bindings.")
    set(USING_BINDINGS_PERL TRUE)
  endif()
  add_build_config(USING_BINDINGS_PERL "bindings_perl")
endif()

if(ENABLE_BINDINGS_PHP)
  find_program(PHP_EXECUTABLE NAMES php)
  # find_package(PHPModules OPTIONAL_COMPONENTS component1)
  if(NOT PHP_EXECUTABLE)
    message(STATUS "Missing php interpreter. Disabling php bindings.")
  elseif(PHPModules_REQUIRED_MISSING)
    message(STATUS "Missing some PHP modules. Disabling PHP bindings.")
  else()
    message(STATUS "PHP interperter and modules found. Enbling php bindings.")
    set(USING_BINDINGS_PHP TRUE)
  endif()
  add_build_config(USING_BINDINGS_PHP "bindings_php")
endif()

#
# There is a libzip cmake module, but it expects to find the zipcmp tool which
# (on Fedora) is part of a separate libzip-tools package and may or may not be
# installed.  Use pkg-config which only cares about the presence of the library.
#
# libzip: fedora:libzip-devel debian:libzip-dev
pkg_check_modules(LIBZIP "libzip" REQUIRED IMPORTED_TARGET)

# liblzo: fedora:lzo-devel debian:liblzo2-dev
pkg_check_modules(LZO2 "lzo2" REQUIRED IMPORTED_TARGET)

# taglib: fedora:taglib-devel debian:libtag1-dev
pkg_check_modules(TAGLIB "taglib>=1.11.1" REQUIRED IMPORTED_TARGET)
add_build_config(PkgConfig::TAGLIB "taglib")

# samplerate: fedora:libsamplerate-devel debian:libsamplerate0-dev
pkg_check_modules(SAMPLERATE samplerate REQUIRED IMPORTED_TARGET)

# MacPorts doesn't include the SoundTouch cmake module, but it does have a
# pkg-config file, so use pkg-config everywhere for consistency.
#
# soundtouch: fedora:soundtouch-devel debian:libsoundtouch-dev
pkg_check_modules(SoundTouch "soundtouch>=1.8.0" REQUIRED IMPORTED_TARGET)

#
# Packages that are conditionally required
#
if(ENABLE_X11
   AND NOT APPLE
   AND NOT CMAKE_CROSSCOMPILING)
  find_package(X11)
  if(TARGET X11::X11)
    if(NOT TARGET X11::Xrandr)
      message(FATAL_ERROR "X11 is missing Xrandr support.")
    endif()
    if(NOT TARGET X11::Xrender)
      message(FATAL_ERROR "X11 is missing Xrender support.")
    endif()
    if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
      if(NOT TARGET X11::Xext)
        message(FATAL_ERROR "X11 is missing Xext support.")
      endif()
      if(NOT TARGET X11::Xxf86vm)
        message(FATAL_ERROR "X11 is missing Xxf86vm support.")
      endif()
    endif()
    target_compile_definitions(X11::X11 INTERFACE USING_X11)
    add_build_config(X11_FOUND "x11")
  endif()
endif()

if(QT_OPENGL_ES_2 STREQUAL true OR QT_FEATURE_opengles2 GREATER 0)
  message(STATUS "  OpenGL ES found")
  set(_QT_HAS_GLES ON)
endif()
if(_QT_HAS_GLES)
  pkg_check_modules(GLES2 IMPORTED_TARGET glesv2)
  add_build_config(PkgConfig::GLES2 "opengl-es2")
  if(TARGET PkgConfig::GLES2)
    target_compile_definitions(PkgConfig::GLES2 INTERFACE USING_OPENGL)
    set(_HAVE_GL_OR_GLES ON)
  endif()
  pkg_check_modules(EGL IMPORTED_TARGET egl)
  add_build_config(PkgConfig::EGL "egl")
  if(TARGET PkgConfig::EGL)
    target_compile_definitions(PkgConfig::EGL INTERFACE USING_EGL)
  endif()
else(_QT_HAS_GLES)
  find_package(OpenGL OPTIONAL_COMPONENTS EGL)
  add_build_config(OpenGL::GL "opengl")
  add_build_config(OpenGL::EGL "egl")
  if(TARGET OpenGL::GL)
    target_compile_definitions(OpenGL::GL INTERFACE USING_OPENGL)
    set(_HAVE_GL_OR_GLES ON)
  endif()
  if(TARGET OpenGL::EGL)
    target_compile_definitions(OpenGL::EGL INTERFACE USING_EGL)
  endif()
endif(_QT_HAS_GLES)

if(ENABLE_VULKAN)
  find_package(Vulkan)
  add_build_config(Vulkan::Vulkan "vulkan")
  if(TARGET Vulkan::Vulkan)
    target_compile_definitions(Vulkan::Vulkan INTERFACE USING_VULKAN)
    get_property(
      _guiprops
      TARGET Qt${QT_VERSION_MAJOR}::Gui
      PROPERTY INTERFACE_QT_ENABLED_FEATURES)
    if(ENABLE_GLSLANG
       AND "vulkan" IN_LIST _guiprops
       AND NOT CMAKE_CROSSCOMPILING)
      list(APPEND CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR})
      pkg_check_modules(GLSLANG IMPORTED_TARGET glslang)
      add_build_config(PkgConfig::GLSLANG "libglslang")
      if(TARGET PkgConfig::GLSLANG)
        pkg_check_modules(Spirv spirv REQUIRED IMPORTED_TARGET)
        pkg_check_modules(SpirvTools SPIRV-Tools REQUIRED IMPORTED_TARGET)
        target_compile_definitions(Vulkan::Vulkan INTERFACE USING_GLSLANG)
        target_link_libraries(
          Vulkan::Vulkan INTERFACE PkgConfig::GLSLANG PkgConfig::Spirv
                                   PkgConfig::SpirvTools)
        if(LSB_RELEASE_ID STREQUAL "fedora"
           OR (LSB_RELEASE_ID STREQUAL "ubuntu" AND LSB_RELEASE_VERSION_ID
                                                    VERSION_GREATER_EQUAL 20.04
              ))
          target_link_libraries(Vulkan::Vulkan INTERFACE MachineIndependent
                                                         GenericCodeGen)
        endif()
      endif()
      list(REMOVE_ITEM CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR})
    endif()
  endif()
endif()

if(ENABLE_WAYLAND AND TARGET Qt${QT_VERSION_MAJOR}::GuiPrivate)
  pkg_check_modules(WAYLAND_CLIENT wayland-client IMPORTED_TARGET)
  add_build_config(PkgConfig::WAYLAND_CLIENT "waylandextras")
  if(TARGET PkgConfig::WAYLAND_CLIENT)
    target_compile_definitions(PkgConfig::WAYLAND_CLIENT
                               INTERFACE USING_WAYLANDEXTRAS)
  endif()
endif()

if(NOT TARGET Vulkan::Vulkan AND NOT _HAVE_GL_OR_GLES)
  message(FATAL_ERROR "Require one of OpenGL or Vulkan. None found!")
endif()

# alsa: fedora:alsa-lib-devel debian:libasound2-dev
if(ENABLE_AUDIO_ALSA)
  # Can't call find_package("ALSA") on OSX. Use pkg_check_modules. 1.0.16
  # contains SND_PCM_NO_AUTO_RESAMPLE
  pkg_check_modules(ALSA "alsa>=1.0.16" IMPORTED_TARGET)
  add_build_config(PkgConfig::ALSA "alsa")
  if(ALSA_FOUND)
    target_compile_definitions(PkgConfig::ALSA INTERFACE USING_ALSA)
    set(CONFIG_AUDIO_ALSA TRUE)
  endif()
endif()

if(ENABLE_AUDIO_OSS)
  find_file(OSS_HEADER NAMES soundcard.h sys/soundcard.h)
  if(OSS_HEADER)
    add_build_config(TRUE "oss")
    add_library(mythtv_oss INTERFACE)
    target_compile_definitions(mythtv_oss INTERFACE USING_OSS)
    set(CONFIG_AUDIO_OSS TRUE)
  endif()
endif()

# pulseaudio: fedora:pulseaudio-libs-devel debian:libpulse-dev
if(ENABLE_AUDIO_PULSE)
  # Can't call find_package("PulseAudio") on OSX.
  pkg_check_modules(PULSEAUDIO libpulse IMPORTED_TARGET)
  add_build_config(PkgConfig::PULSEAUDIO "pulse")
  if(PULSEAUDIO_FOUND)
    target_compile_definitions(PkgConfig::PULSEAUDIO INTERFACE USING_PULSE)
    if(ENABLE_AUDIO_PULSEOUTPUT)
      target_compile_definitions(PkgConfig::PULSEAUDIO
                                 INTERFACE USING_PULSEOUTPUT)
    endif()
  endif()
endif()

# jack: fedora:jack-audio-connection-kit-devel debian:n/a
if(ENABLE_AUDIO_JACK)
  pkg_check_modules(JACK "jack" IMPORTED_TARGET)
  add_build_config(PkgConfig::JACK "jack")
  if(JACK_FOUND)
    target_compile_definitions(PkgConfig::JACK INTERFACE USING_JACK)
  endif()
endif()

# v4l2: fedora:kernel-headers debian:linux-libc-dev
if(ENABLE_V4L2)
  # FreeBSD provides V4L2 library, but not videodev2.h header file.
  message(STATUS "Checking for v4l header file")
  find_file(VIDEODEV2_HEADER linux/videodev2.h)
  if(VIDEODEV2_HEADER)
    message(STATUS "  Found ${VIDEODEV2_HEADER}")
    pkg_check_modules(V4L2 "libv4l2" IMPORTED_TARGET)
    add_build_config(PkgConfig::V4L2 "v4l2")
    if(TARGET PkgConfig::V4L2)
      set(CONFIG_V4L2 TRUE)
      target_compile_definitions(PkgConfig::V4L2 INTERFACE USING_V4L2)
      if(TARGET PkgConfig::DRM
         AND NOT APPLE
         AND NOT WIN32)
        add_build_config(TRUE "v4l2prime")
        target_compile_definitions(PkgConfig::V4L2 INTERFACE USING_V4L2PRIME)
      endif()
    endif()
  endif()
endif()

# libcrypto: fedora:openssl-devel debian:libssl-dev
if(ENABLE_LIBCRYPTO)
  pkg_check_modules(LIBCRYPTO "libcrypto" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBCRYPTO "libcrypto")
  if(LIBCRYPTO_FOUND)
    target_compile_definitions(PkgConfig::LIBCRYPTO INTERFACE USING_LIBCRYPTO)
  endif()
endif()

# ~~~
# dns_sd: fedora:avahi-compat-libdns_sd-devel
#         debian:libavahi-compat-libdnssd-dev
# ~~~
if(ENABLE_LIBDNS_SD)
  pkg_check_modules(LIBDNS_SD "libdns_sd" IMPORTED_TARGET)
  if(NOT LIBDNS_SD)
    pkg_check_modules(LIBDNS_SD "avahi-compat-libdns_sd" IMPORTED_TARGET)
  endif()
  add_build_config(PkgConfig::LIBDNS_SD "libdns_sd")
  if(LIBDNS_SD_FOUND)
    target_compile_definitions(PkgConfig::LIBDNS_SD INTERFACE USING_LIBDNS_SD)
    if(LIBCRYPTO_FOUND)
      target_compile_definitions(PkgConfig::LIBDNS_SD INTERFACE USING_AIRPLAY)
    endif()
    set(CONFIG_LIBDNS_SD TRUE)
  endif()
endif()

# libsystemd: fedora:systemd-devel debian:libsystemd-dev
if(ENABLE_SYSTEMD_NOTIFY OR ENABLE_SYSTEMD_JOURNAL)
  pkg_check_modules(SYSTEMD "libsystemd" IMPORTED_TARGET)
  if(SYSTEMD_FOUND AND ENABLE_SYSTEMD_JOURNAL)
    find_file(SYSTEMD_JOURNALD "systemd/sd-journal.h"
              PATHS ${SYSTEMD_INCLUDE_PATH})
    add_build_config(SYSTEMD_JOURNALD "systemd_journal")
    set(CONFIG_SYSTEMD_JOURNAL ON)
  endif()
  if(SYSTEMD_FOUND AND ENABLE_SYSTEMD_NOTIFY)
    find_file(SYSTEMD_NOTIFY "systemd/sd-daemon.h"
              PATHS ${SYSTEMD_INCLUDE_PATH})
    add_build_config(SYSTEMD_NOTIFY "systemd_notify")
    set(CONFIG_SYSTEMD_NOTIFY ON)
  endif()
endif()

if(ENABLE_HDHOMERUN)
  find_package(HDHomerun)
  add_build_config(HDHomerun::HDHomerun "hdhomerun")
endif()
if(ENABLE_VBOX)
  message(STATUS "Adding vbox support")
  add_library(mythtv_vbox INTERFACE)
  add_build_config(TRUE "vbox")
  target_compile_definitions(mythtv_vbox INTERFACE USING_VBOX)
endif()
if(ENABLE_CETON)
  message(STATUS "Adding ceton support")
  add_library(mythtv_ceton INTERFACE)
  add_build_config(TRUE "ceton")
  target_compile_definitions(mythtv_ceton INTERFACE USING_CETON)
endif()

if(ENABLE_SATIP)
  message(STATUS "Adding satip support")
  add_library(mythtv_satip INTERFACE)
  add_build_config(TRUE "satip")
  target_compile_definitions(mythtv_satip INTERFACE USING_SATIP)
endif()
if(ENABLE_DVB)
  if(NOT APPLE AND NOT WIN32)
    find_file(DVB_FRONTEND_H "linux/dvb/frontend.h")
    if(DVB_FRONTEND_H)
      message(STATUS "Adding dvb support (method 1)")
      add_library(mythtv_dvb INTERFACE)
      add_build_config(TRUE "dvb")
      target_compile_definitions(mythtv_dvb INTERFACE USING_DVB)
    elseif(MYTH_DVB_PATH)
      find_file(DVB_FRONTEND_H "linux/dvb/frontend.h" PATHS "${MYTH_DVB_PATH}")
      if(DVB_FRONTEND_H)
        message(STATUS "Adding dvb support (method 2)")
        add_library(mythtv_dvb INTERFACE)
        add_build_config(TRUE "dvb")
        target_compile_definitions(mythtv_dvb INTERFACE USING_DVB)
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
    target_compile_definitions(mythtv_asi INTERFACE USING_ASI)
  endif()
endif()

# libcec: fedora:libcec-devel debian:libcec-dev
if(ENABLE_LIBCEC)
  pkg_check_modules(LibCEC "libcec" IMPORTED_TARGET)
  add_build_config(PkgConfig::LibCEC "libcec")
  if(TARGET PkgConfig::LibCEC)
    target_compile_definitions(PkgConfig::LibCEC INTERFACE USING_LIBCEC)
  endif()
endif()

if(APPLE)
  # So far, only OS X 10.4 has this as a non-private framework
  if(EXISTS /System/Library/Frameworks/DiskArbitration.framework/Headers)
    set(DARWIN_DA TRUE)
  endif()

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

  # Take our cue on videotolbox from ffmpeg
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
    OUTPUT_VARIABLE _output
    ERROR_QUIET)
    message(STATUS "Looking for videotoolbox: ${_output}")
  string(FIND ${_output} videotoolbox VIDEOTOOLBOX_OFFSET)
  if(VIDEOTOOLBOX_OFFSET EQUAL -1)
    unset(APPLE_VIDEOTOOLBOX_LIBRARY)
  endif()

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

if(ENABLE_JOYSTICK_MENU)
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(STATUS "Checking for library 'udev'")
    find_library(LINUX_UDEV NAMES udev libudev libudev.so libudev.so.1)
    if(LINUX_UDEV)
      message(STATUS "  Found ${LINUX_UDEV}")
      add_library(joystick INTERFACE)
      add_build_config(TRUE "joystick_menu")
      target_compile_definitions(joystick INTERFACE USING_JOYSTICK_MENU)
      target_link_libraries(joystick INTERFACE ${LINUX_UDEV})
    else()
      message(STATUS "  Can't find the udev library. Disabling joystick.")
    endif()
  endif()
endif()

if(ENABLE_LIRC)
  # This uses the socket interface. No libs necessary
  message(STATUS "Adding lirc support")
  add_library(lirc INTERFACE)
  add_build_config(TRUE "lirc")
  target_compile_definitions(lirc INTERFACE USING_LIRC)
endif()

# Use MMAL as a proxy for Raspberry Pi support.  Notes: MMAL didn't work well on
# 64bit systems. It has been deprecated in Debian 11 Bullseye in favor of
# V4L2(?).
if(ENABLE_MMAL)
  cmake_push_check_state(RESET)
  # ~~~
  # Raspbian  9: /opt/vc/include
  # Raspbian 11: /usr/include
  # ~~~
  set(CMAKE_REQUIRED_FLAGS -I/opt/vc/include)
  check_include_file(interface/mmal/mmal.h HAVE_MMAL_H)
  set(CMAKE_REQUIRED_LIBRARIES mmal_core mmal_util mmal_vc_client bcm_host)
  check_symbol_exists(mmal_port_connect interface/mmal/mmal.h
                      HAVE_MMAL_PORT_CONNECT)
  cmake_pop_check_state()

  if(HAVE_MMAL_H AND HAVE_MMAL_PORT_CONNECT)
    message(STATUS "Adding mmal support")
    add_library(mythtv_mmal INTERFACE)
    add_build_config(TRUE "mmal")
    target_include_directories(mythtv_mmal SYSTEM INTERFACE /opt/vc/include)
    target_compile_definitions(mythtv_mmal INTERFACE USING_MMAL)
    target_link_libraries(mythtv_mmal INTERFACE -L/opt/vc/lib mmal vchostif
                                                vcsm vchiq_arm)
  endif()
endif()

if(ENABLE_MEDIACODEC AND ANDROID)
  message(STATUS "Adding mediacodec support")
  add_library(mediacodec INTERFACE)
  target_compile_definitions(mediacodec INTERFACE USING_MEDIACODEC)
endif()

if(ENABLE_MHEG)
  message(STATUS "Adding mheg support")
  add_library(mythtv_mheg INTERFACE)
  add_build_config(TRUE "mheg")
  target_compile_definitions(mythtv_mheg INTERFACE USING_MHEG)
endif()

if(ENABLE_FIREWIRE AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  pkg_check_modules(LibAVC1394 libavc1394 IMPORTED_TARGET)
  pkg_check_modules(LibIEC61883 libiec61883 IMPORTED_TARGET)
  if(TARGET PkgConfig::LibAVC1394 AND TARGET PkgConfig::LibIEC61883)
    message(STATUS "Adding linux firewire support")
    target_compile_definitions(PkgConfig::LibAVC1394 INTERFACE USING_FIREWIRE)
  else()
    set(ENABLE_FIREWIRE OFF)
  endif()
endif()
if(ENABLE_FIREWIRE)
  add_build_config(TRUE "firewire")
endif()
