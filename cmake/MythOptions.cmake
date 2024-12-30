#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# These options are used both the SuperPackage and by MythTV.  The superpackages
# uses them to determine the arguments to pass to the FFmpeg configure command.
# MythTV and its plugins use them for optionally compiling parts of core.
#

#
# Default Install Prefix
#
set(MYTH_DEFAULT_PREFIX
    ""
    CACHE PATH "Default install prefix if not specified on the command line.")
set(MYTH_DEFAULT_LIBS_PREFIX
    ""
    CACHE PATH
          "Default library install prefix if not specified on the command line."
)
set(MYTH_RUN_PREFIX
    ""
    CACHE PATH
          "The prefix where MythTV is expected to be at runtime.  This may differ from MYTH_DEFAULT_PREFIX or CMAKE_INSTALL_PREFIX for packagers."
)

# Location for downloaded tarballs.
#
# Note: If left blank, the value of "${PROJECT_SOURCE_DIR}/tarballs" will be
# used.
set(TARBALL_DIR
    ""
    CACHE PATH "Default location for downloading external project tarballs.")

# Qt5 or Qt6
set(QT_VERSION_MAJOR
    "5"
    CACHE STRING "Qt verion to use/build.")

#
# Build applications for Frontend or Backend.
#
option(ENABLE_FRONTEND "Enable frontend support." ON)
option(ENABLE_BACKEND "Enable backend support." ON)

#
# Build bindings
#
option(ENABLE_BINDINGS_PYTHON "Install the Python bindings" ON)
option(ENABLE_BINDINGS_PERL "Install the Perl bindings" ON)
option(ENABLE_BINDINGS_PHP "Install the PHP bindings" ON)
set(MYTH_BINDINGS_INSTALL_ROOT
    ""
    CACHE PATH "The root direcory for bindings installation")
set(MYTH_PERL_CONFIG_OPTS
    ""
    CACHE STRING "Options to pass to PERL when creating bindings")

#
# Language Interpreter Locations
#
# Where to look first when searching for a usable versions of Python3, Perl, or
# PHP.  Setting these variables shouldn't be necessary unless you need to
# explicitly override the language interpreters found by CMake.
set(Python3_ROOT_DIR
    ""
    CACHE STRING "The directory where Python3 is installed.")
set(Perl_ROOT_DIR
    ""
    CACHE STRING "The directory where Perl is installed.")
set(PHP_ROOT_DIR
    ""
    CACHE STRING "The directory where PHP is installed.")

#
# Audio Options
#
option(ENABLE_ALSA "Enable ALSA audio support." ON)
option(ENABLE_AUDIO_ALSA "Enable ALSA audio support" ON)
option(ENABLE_AUDIO_JACK "Enable JACK audio support" ON)
option(ENABLE_AUDIO_OSS "Enable OSS audio support" ON)
option(ENABLE_AUDIO_PULSE "Enable PulseAudio audio support" ON)
option(ENABLE_AUDIO_PULSEOUTPUT "Enable PulseAudio audio output support" ON)

#
# Video Display Options
#
option(ENABLE_X11 "Enable X11 support" ON)
option(ENABLE_VULKAN "Enable Vulkan rendering." ON)
option(ENABLE_GLSLANG
       "Enable runtime GLSL compilation for Vulkan (development only)" ON)
option(ENABLE_WAYLAND "Enable Wayland support" ON)

#
# Video Hardware Acceleration
#
option(ENABLE_D3D "Enable Direct3D drawing interface on windows" OFF)
option(ENABLE_DXVA2 "Enable hardware accelerated decoding on windows" OFF)
option(ENABLE_MEDIACODEC "Enable accelerated decoding on android" ON)
option(ENABLE_MMAL "Enable hardware accelerated decoding on Raspberry Pi" ON)
option(ENABLE_NVDEC "Enable (CUVID) hardware accelerated video decoding" ON)
option(ENABLE_VAAPI "Enable VAAPI hardware accelerated video decoding" ON)
option(ENABLE_VDPAU "Enable NVidia VDPAU hardware acceleration." ON)

#
# Backend Video Collection Methods
#
option(ENABLE_ASI "Enable support for DVEO ASI recorder" ON)
option(ENABLE_CETON "Enable support for Ceton cards" ON)
option(ENABLE_HDHOMERUN "Enable support for HDHomeRun boxes." ON)
option(ENABLE_SATIP "Enable support for Sat>IP" ON)
option(ENABLE_V4L2 "Enable libv4l2 support." ON)
option(ENABLE_VBOX "Enable support for V@Box TV Gateway boxes" ON)
option(ENABLE_DVB "Enable DVB support" ON)
set(MYTH_DVB_PATH
    ""
    CACHE PATH
          "location of directory containing the file 'linux/dvb/frontend.h'")

#
# Codecs
#
option(ENABLE_LIBAOM "Enable AV1 video encoding/decoding via libaom" ON)
option(ENABLE_LIBASS "Enable libass SSA/ASS subtitle support" ON)
option(ENABLE_LIBDAV1D "Enable AV1 decoding via libdav1d" ON)
option(ENABLE_MP3LAME "Enable MP3Lame support." ON)
option(ENABLE_VPX "Enable VP8 encoding via libvpx." ON)
option(ENABLE_X264 "Enable H.264 encoding via x264." ON)
option(ENABLE_X265 "Enable H.265 encoding via x265." ON)
option(ENABLE_XML2 "Enable libxml2 support." ON)
option(ENABLE_XVID
       "Enable Xvid encoding via xvidcore, native MPEG-4/Xvid encoder exists."
       ON)

#
# Firewire support
#
option(ENABLE_FIREWIRE "Enable support for FireWire cable boxes." ON)
set(MYTH_FIREWIRE_SDK
    "$ENV{HOME}/Frameworks"
    CACHE PATH "Specify location for mac FireWire SDK [mac only]")
if(NOT EXISTS ${MYTH_FIREWIRE_SDK})
  set(MYTH_FIREWIRE_SDK "")
endif()

#
# Miscellaneous features
#
option(ENABLE_BDJAVA "Enable BD-J support for Blu-ray discs" ON)
option(ENABLE_DRM "Enable Direct Rendering Manager (DRM) support" ON)
option(ENABLE_FONTCONFIG "Enable libfontconfig support." ON)
option(ENABLE_FREETYPE "Enable libfreetype support." ON)
option(
  ENABLE_GNUTLS
  "enable gnutls, needed for https support if openssl, libtls or mbedtls is not used."
  ON)
option(ENABLE_JOYSTICK_MENU "Enable support joystick menu." ON)
option(ENABLE_LIBCEC "Enable libcec support" ON)
option(ENABLE_LIBCRYPTO "Enable use of the OpenSSL cryptographic library" ON)
option(ENABLE_LIBDNS_SD "Enable DNS Service Discovery (Bonjour/Zeroconf/Avahi)"
       ON)
option(ENABLE_LIRC "Enable lirc support (Infrared Remotes)" ON)
option(ENABLE_MHEG "Enable MHEG support." ON)
option(ENABLE_SDL2 "Enable SDL2 support." ON)
option(ENABLE_SYSTEMD_JOURNAL "Enable systemd journal support." ON)
option(ENABLE_SYSTEMD_NOTIFY "Enable systemd notify support." ON)
option(
  ENABLE_EXIV2_DOWNLOAD
  "Build latest exiv2 instead of embedded copy.  This only afects native builds. Android/Windows builds will always downloded exiv2."
  OFF)
option(
  CONFIG_FORCE_LOGLONG
  "Use long loging format for the console (i.e. show file, line number, etc.)."
  OFF)

#
# Miscellaneous compilation options
#
set(CMAKE_BUILD_TYPE
    "Debug"
    CACHE
      STRING
      "The default type of build. Choices are Debug, Release, RelWithDebInfo, or MinSizeRel"
)
option(ENABLE_STRICT_BUILD_ORDER
       "Force all libraries to be built in the same order every time." ON)
option(
  ENABLE_CLANG_TIDY
  "Run clang-tidy as part of each compile, to check for typical programming errors."
  OFF)
option(
  ENABLE_CPPCHECK
  "Run cppcheck as part of each compile, to check for typical programming errors."
  ON)
option(ENABLE_HIDE_SYMBOLS "Hide all symbol names by default" ON)
option(ENABLE_LTO "Enable link time optimization" ON)
option(ENABLE_SELF_TEST "Enable some self tests for the CMake scripts." OFF)
option(MYTH_VERSIONED_EXTENSIONS
       "Add version number to libray file name (same as original android build)"
       ON)

#
# Library build instructions
#
# The first option affect the cmake configuration stage, and the seconds affects
# the cmake build stage.
#
option(
  LIBS_USE_INSTALLED
  "Reuse libraries installed by previous runs of cmake.  Don't generate the framework to rebuild a library if it is already installed."
  ON)
option(LIBS_ALWAYS_REBUILD "Rebuild libraries on every call to --build." OFF)

#
# Library install locations.  Using configure/make these will be compiled every
# time. Setting the following to ON will put these into the LIBS directory so
# they only need to be compiled once.
#
option(LIBS_INSTALL_EXIV2 "Install the exiv2 library in the libs directory" ON)
option(LIBS_INSTALL_UDFREAD
       "Install the libudfread library in the libs directory" ON)
option(LIBS_INSTALL_FFMPEG "Install the FFmpeg libraries in the libs directory"
       ON)

#
# Plugins
#
option(MYTH_BUILD_PLUGINS "Build and install the MythTV plugins." ON)

#
# Admin tools
#
option(MYTH_BUILD_THEMESTRING_TOOL
       "Build the tool for extracting strings from themes." OFF)

#
# Android Related Options
#
if(ANDROID)
  # As per:
  # https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-android
  #
  # Note: Any of these cache variable can be overridden on the command line.
  set(ANDROID_MIN_SDK_VERSION
      "21"
      CACHE
        STRING
        "\
Android minimum SDK version.  This sets the minimum \
Java ABI level to use when compiling for android.  MythTV \
can get by with this minimum level of features in the API.")
  set(ANDROID_TARGET_SDK_VERSION
      "29"
      CACHE
        STRING
        "\
Android target SDK version.  This sets the target \
Java ABI level to use when compiling for android.  MythTV \
will use features past the minimum API level up to this \
API level if they are present.")
  set(ANDROID_PLATFORM
      ""
      CACHE
        STRING
        "\
The Android platform target for building.  If empty, this \
will be set based on the value of CMAKE_SYSTEM_VERSION.  \
Available values can be found in $ENV{HOME}/Android/Sdk/platforms")
  # ~~~
  # Minimum SDK Build Tools versions:
  # Qt 5.15.11 (aka Gradle Plugin 7.0.2) requires 30.0.2
  # Qt 6.7.2 (aka Gradle Plugin 7.4.1) requires 30.0.3
  # ~~~
  set(ANDROID_SDK_BUILD_TOOLS_REVISION
      "30.0.2"
      CACHE
        STRING
        "\
Android build tools version.  This specifies which compiler \
and other tools to use when building the android specific \
bits of the project.  Available versions can be found in \
$ENV{HOME}/Android/Sdk/build-tools")
  set(ANDROID_SDK
      ""
      CACHE PATH "\
Location of the Android SDK.  If empty the code will attempt \
to figure it out from the NDK path.")
  set(CMAKE_ANDROID_NDK
      $ENV{HOME}/Android/Sdk/ndk/26.1.10909125
      CACHE
        PATH
        "\
Default Android NDK to use.  This sets the C/C++ ABI \
level to use when compiling for android.  Available \
versions can be found in $ENV{HOME}/Android/Sdk/ndk")
  # Valid values are arm64-v8a, armeabi-v7a, x86, and x86_64.
  set(CMAKE_ANDROID_ARCH_ABI
      arm64-v8a
      CACHE STRING "Default Android ABI to target")
  option(MYTH_BUILD_ANDROID_PLUGINS
         "Install the plugins as part of the built apk." ON)

  #
  # Enable or disable signing of the built android package.  The SIGN_APK
  # variable enables/disables signing.  The rest of the variables are arguments
  # to the signing process.
  #
  option(QT_ANDROID_SIGN_APK "Sign the package." OFF)
  set(QT_ANDROID_KEYSTORE_PATH
      ""
      CACHE PATH "The path to the local android key store.")
  set(QT_ANDROID_KEYSTORE_ALIAS
      ""
      CACHE STRING "The android key to use to sign the package.")
  set(QT_ANDROID_KEYSTORE_STORE_PASS
      ""
      CACHE STRING "The password for the android key store.")
  set(QT_ANDROID_KEYSTORE_KEY_PASS
      ""
      CACHE
        STRING
        "The password for the specified key (if it is different from the password for the android key store)."
  )
endif()

#
# JAVA
#
# Needed for libmythbluray and android compiles.  These will be added to the
# environment for the necessary commands if there isn't already a JDK_HOME
# environment variable.
#
set(MYTH_JAVA_HOME
    ""
    CACHE
      PATH
      "Path to JDK home directory. This will be used if the JAVA_HOME environment variable isn't set."
)

#
# Load any user overrides to these values.
#
set(MYTH_USER_OVERRIDES1 "$ENV{HOME}/.config/MythTV/BuildOverridesPre.cmake")
set(MYTH_USER_OVERRIDES2 "$ENV{HOME}/.config/MythTV/BuildOverridesPost.cmake")
