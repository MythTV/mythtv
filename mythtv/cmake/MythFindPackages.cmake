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
      OUTPUT_VARIABLE _output
      ERROR_QUIET)
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
    Python3 3.8
    COMPONENTS Interpreter
    REQUIRED)
  if(TARGET Python3::Interpreter)
    message(STATUS "Found python version ${Python3_VERSION}.")
    find_package(
      Python3Modules
      COMPONENTS
        MySQLdb
        lxml
        requests
      OPTIONAL_COMPONENTS
        pip>=23.0.1
        wheel)
  endif()

  if(NOT TARGET Python3::Interpreter)
    message(STATUS "Missing python interpreter. Disabling python bindings.")
  elseif(Python3Modules_REQUIRED_MISSING)
    message(
      STATUS "Missing some required python modules. Disabling python bindings.")
  else()
    message(
      STATUS "Python interpreter and modules found. Enabling python bindings.")
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
    message(STATUS "Found perl version ${PERL_VERSION_STRING}.")
    find_package(
      PerlModules
      COMPONENTS
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
    message(
      STATUS "Perl interpreter and modules found. Enabling perl bindings.")
    set(USING_BINDINGS_PERL TRUE)
  endif()
  add_build_config(USING_BINDINGS_PERL "bindings_perl")
endif()

if(ENABLE_BINDINGS_PHP)
  find_program(PHP_EXECUTABLE NAMES php)
  if(PHP_EXECUTABLE)
    execute_process(COMMAND ${PHP_EXECUTABLE} --version OUTPUT_VARIABLE _output)
    if("${_output}" MATCHES "PHP ([0-9\.]+).*")
      set(PHP_VERSION ${CMAKE_MATCH_1})
    endif()
    message(STATUS "Found PHP version ${PHP_VERSION}.")
    # There currently aren't any required PHP packages.
    #
    # find_package(PHPModules OPTIONAL_COMPONENTS mysqlnd geos sodium)
  endif()
  if(NOT PHP_EXECUTABLE)
    message(STATUS "Missing php interpreter. Disabling php bindings.")
  elseif(PHPModules_REQUIRED_MISSING)
    message(STATUS "Missing some PHP modules. Disabling PHP bindings.")
  else()
    message(STATUS "PHP interpreter and modules found. Enabling php bindings.")
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
    set(CONFIG_X11 ON)
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
  set(CONFIG_OPENGL ${GLES2_FOUND})
  pkg_check_modules(EGL IMPORTED_TARGET egl)
  add_build_config(PkgConfig::EGL "egl")
  set(CONFIG_EGL ${EGL_FOUND})
  add_library(any_opengl INTERFACE)
  if(TARGET PkgConfig::GLES2)
    target_link_libraries(
      any_opengl INTERFACE Qt${QT_VERSION_MAJOR}::OpenGL PkgConfig::GLES2
                           $<TARGET_NAME_IF_EXISTS:PkgConfig::EGL>)
  endif()
else(_QT_HAS_GLES)
  find_package(OpenGL OPTIONAL_COMPONENTS EGL)
  add_build_config(OpenGL::GL "opengl")
  add_build_config(OpenGL::EGL "egl")
  set_if_target_exists(CONFIG_OPENGL OpenGL::GL)
  set_if_target_exists(CONFIG_EGL OpenGL::EGL)
  if(TARGET OpenGL::GL)
    add_library(any_opengl INTERFACE)
    target_link_libraries(
      any_opengl INTERFACE Qt${QT_VERSION_MAJOR}::OpenGL OpenGL::GL
                           $<TARGET_NAME_IF_EXISTS:OpenGL::EGL>)
  endif()
endif(_QT_HAS_GLES)

if(ENABLE_VULKAN)
  find_package(Vulkan)
  add_build_config(Vulkan::Vulkan "vulkan")
  if(TARGET Vulkan::Vulkan)
    set(CONFIG_VULKAN TRUE)
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
        set(CONFIG_LIBGLSLANG TRUE)
        pkg_check_modules(Spirv spirv REQUIRED IMPORTED_TARGET)
        pkg_check_modules(SpirvTools SPIRV-Tools REQUIRED IMPORTED_TARGET)
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
  set(CONFIG_WAYLANDEXTRAS ${WAYLAND_CLIENT_FOUND})
endif()

if(NOT TARGET Vulkan::Vulkan AND NOT TARGET any_opengl)
  message(FATAL_ERROR "Require one of OpenGL or Vulkan. None found!")
endif()

# alsa: fedora:alsa-lib-devel debian:libasound2-dev
if(ENABLE_AUDIO_ALSA)
  # Can't call find_package("ALSA") on OSX. Use pkg_check_modules. 1.0.16
  # contains SND_PCM_NO_AUTO_RESAMPLE
  pkg_check_modules(ALSA "alsa>=1.0.16" IMPORTED_TARGET)
  add_build_config(PkgConfig::ALSA "alsa")
  set(CONFIG_AUDIO_ALSA ${ALSA_FOUND})
endif()

if(ENABLE_AUDIO_OSS)
  find_file(OSS_HEADER NAMES soundcard.h sys/soundcard.h)
  if(OSS_HEADER)
    add_build_config(TRUE "oss")
    add_library(mythtv_oss INTERFACE)
    set(CONFIG_AUDIO_OSS TRUE)
  endif()
endif()

# pulseaudio: fedora:pulseaudio-libs-devel debian:libpulse-dev
if(ENABLE_AUDIO_PULSE)
  # Can't call find_package("PulseAudio") on OSX.
  pkg_check_modules(PULSEAUDIO libpulse IMPORTED_TARGET)
  add_build_config(PkgConfig::PULSEAUDIO "pulse")
  if(PULSEAUDIO_FOUND)
    set(CONFIG_AUDIO_PULSE TRUE)
    set(CONFIG_AUDIO_PULSEOUTPUT ${ENABLE_AUDIO_PULSEOUTPUT})
  endif()
endif()

# jack: fedora:jack-audio-connection-kit-devel debian:n/a
if(ENABLE_AUDIO_JACK)
  pkg_check_modules(JACK "jack" IMPORTED_TARGET)
  add_build_config(PkgConfig::JACK "jack")
  set(CONFIG_AUDIO_JACK ${JACK_FOUND})
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
      if(TARGET PkgConfig::DRM
         AND NOT APPLE
         AND NOT WIN32)
        add_build_config(TRUE "v4l2prime")
        set(CONFIG_V4L2PRIME TRUE)
      endif()
    endif()
  endif()
endif()

# libcrypto: fedora:openssl-devel debian:libssl-dev
if(ENABLE_LIBCRYPTO)
  pkg_check_modules(LIBCRYPTO "libcrypto" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBCRYPTO "libcrypto")
  set(CONFIG_LIBCRYPTO ${LIBCRYPTO_FOUND})
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
    set(CONFIG_LIBDNS_SD TRUE)
    set(CONFIG_AIRPLAY ${LIBCRYPTO_FOUND})
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

# libcec: fedora:libcec-devel debian:libcec-dev
if(ENABLE_LIBCEC)
  pkg_check_modules(LibCEC "libcec" IMPORTED_TARGET)
  add_build_config(PkgConfig::LibCEC "libcec")
  set(CONFIG_LIBCEC ${LibCEC_FOUND})
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
    set(CONFIG_MMAL TRUE)
    target_include_directories(mythtv_mmal SYSTEM INTERFACE /opt/vc/include)
    target_link_libraries(mythtv_mmal INTERFACE -L/opt/vc/lib mmal vchostif
                                                vcsm vchiq_arm)
  endif()
endif()

if(ENABLE_MEDIACODEC AND ANDROID)
  message(STATUS "Adding mediacodec support")
  add_library(mediacodec INTERFACE)
  set(CONFIG_MEDIACODEC TRUE)
endif()

if(ENABLE_MHEG)
  message(STATUS "Adding mheg support")
  add_library(mythtv_mheg INTERFACE)
  add_build_config(TRUE "mheg")
  set(CONFIG_MHEG TRUE)
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
    # udev: fedora:libiec61883-devel debian:libiec61883-dev
    # udev: fedora:libavc1394-devel debian:libavc1394-dev
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
