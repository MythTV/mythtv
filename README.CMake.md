
# COMPILING MYTHTV WITH CMAKE

## Introduction

  Support for building MythTV using "cmake" is an alternative to the
  long standing "make" build rules.  This provides a unified build
  structure for building Linux, android and windows executable using
  the same commands.

  The cmake builds are structured slightly different from the
  traditional make builds.  The top level cmake is essentially a build
  orchestrator, building any libraries that are needed and then
  building mythtv and mythplugins.  This means that building the
  embedded FFmpeg or exiv2 are treated as the equivalent of building
  the mythtv directory or the mythplugins directory.  After being
  built once, subsequent runs of cmake should pick up the libraries
  from the previous build and only build mythtv and mythplugins.

  You will not need to run the traditional `configure` command when
  building with cmake.

## Prerequisites

### All compiles

  You will need to install the following programs:

  - cmake
  - patch
  - ninja-build (ninja on freebsd and osx)

### Compiling for Android/Windows

  If you are building an android or windows target you will need to
  install the following programs:

  - gperf
  - meson
  - perl-FindBin(Fedora) libfindbin-libs-perl(Debian/Ubuntu)
  - perl-IPC-Cmd(Fedora) perl-modules(Debian/Ubuntu)

### Compiling for Android

  Android builds also require you to install a somewhat recent version
  of Android Studio.  Cmake requires ndk version 23.0 or better (from
  August 2021).  The scripts assume that android is installed in
  $(HOME)/Android, but you can change that with a command line
  argument or an local options setting.

### Compiling for Windows

  Windows builds also require you to install the mingw cross compiler
  and some other utility programs.  Install the following packages:

  - byacc
  - flex
  - mingw64-gcc-c++
  - mingw64-libstdc++
  - mingw64-pixman
  - mingw64-readline
  - mingw64-zstd

  On Fedora, you can also install a number of pre-built libraries so
  that they don't have to be compiled as part of building MythTV.
  These are:

  - mingw64-flac
  - mingw64-fontconfig
  - mingw64-freetype
  - mingw64-fribidi
  - mingw64-harfbuzz
  - mingw64-icu
  - mingw64-libogg
  - mingw64-libvorbis
  - mingw64-libzip
  - mingw64-openssl
  - mingw64-pcre2
  - mingw64-taglib
  - mingw64-win-iconv
  - mingw64-zlib

## General

  This cmake build is capable of building both native, android (cross
  compiled), and windows (cross compiled) versions of MythTV using the
  same set of build instructions.

  The traditional MythTV build has a number of external dependencies
  that are embedded into the MythTV source code, and are compiled as
  sub-directories of MythTV. The CMake build compiles and installs
  those as separate projects, isolating them from MythTV, and
  preventing any direct access to internal headers/functions.  Here is
  the list of the dependencies that are built as separate projects for
  a native build, and those that are still built as part of building
  MythTV.

  | Component        | Location   | Built as         | When Built     |
  | :---             | :---       | :---             | :---           |
  | exiv2            | downloaded | Separate Project | system < 1.0   |
  | FFmpeg           | embedded   | Separate Project | always         |
  | libudfread       | embedded   | Separate Project | system < 1.1.1 |
  | nv-codec-headers | embedded   | Separate Project | always         |
  | mythdvdnav       | embedded   | Part of MythTV   | always         |
  | mythbluray       | embedded   | Part of MythTV   | system < 0.9.3 |

  For a cross compilation of Android or Windows build, there is a much
  longer list of dependencies, all of which are downloaded, compiled,
  and installed as separate projects.

  By default, CMake builds will try and compile in support for as many
  optional parts of MythTV as possible.  If the necessary system
  libraries are present, then the cmake build will use them.  (This is
  the opposite of the traditional make build, where all optional
  features were disabled by default.)  If you encounter a problem with
  a feature, you may disable it by adding `-DENABLE_<FEATURE>=OFF` to
  the command line.  See the [options](#options) section for more
  information.

## Native Compiles

  To compile a native version of MythTV, change into the **root**
  directory of your checkout (not the mythtv directory) and issue the
  following commands.  You do not need to run `configure`.

  ```
  $ cmake --preset qt5 -DCMAKE_INSTALL_PREFIX=<install_location>
  $ cmake --build build-qt5
  ```

  The first line will create a new directory named "build-qt5", and
  then install into that directory the framework to compile MythTV
  using the Ninja build tool.  The second line will
  download/compile/install all dependencies, then compile/install
  MythTV, and then compile/install the plugins.  More specifically,
  this will compile/install exiv2, then compile/install FFmpeg. then
  compile/install MythTV, then compile/install the plugins.  If you
  don't specify the CMAKE_INSTALL_PREFIX directory, this will install
  into /usr/local.  Once the initial build has been performed, you can
  compile only the MythTV component by issuing the following
  command(s):

  ```
  $ cmake --build build-qt5/MythTV-prefix/src/MythTV-build
  ```
  or
  ```
  $ cd build-qt5/MythTV-prefix/src/MythTV-build
  $ cmake --build .
  ```

  You can also delete the build directory and recreate it at any time.
  The next time cmake is run in the top level directory it will pick
  up the libraries from the previous build and only build mythtv and
  mythplugins.  This is the equivalent of the traditional build's `git
  clean -xfd ; configure ; make` sequence since all of the build
  artifacts are contained within the build directory.

  If you want to build a qt6 version of MythTV, use these commands
  instead:

  ```
  $ cmake --preset qt6 -DCMAKE_INSTALL_PREFIX=<install_location>
  $ cmake --build build-qt6
  ```

  If you want more control over the build process, look at the first
  few lines of output from the first cmake command to see what
  variables are specified by that preset.  To create a custom build
  without using a preset, you would issue commands like this:

  ```
  $ cmake -S . -B build-qt5 -G Ninja -DCMAKE_INSTALL_PREFIX=<install_location>
  $ cmake --build build-qt5
  ```

  This will use the current directory as the source (-S) directory,
  create a build (-B) directory named build-qt5, and use the Ninja
  generator (-G) to create the build framework in the build-qt5
  directory.  The build directory can be named anything, but
  traditionally begins with the string "build".  The build directory
  can also be located anywhere.  It doesn't need to be a subdirectory
  of the source directory.

  CMake allows you to use multiple different
  [generators](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html)
  to convert the cmake instructions into native build instructions.
  The three I have tested with are Ninja, 'Unix Makefiles', and Xcode
  (on MacOS X).  To use traditional unix makefiles, substitute
  'Unix Makefiles' (with the quotes) in the above example commands.

  To set the [build
  type](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html),
  you can add the `-DCMAKE_BUILD_TYPE=<value>` argument to the initial
  cmake command.  The possible values and the compile options they add
  are:

     | Value          | Compiler Options |
     | :---           | :---             |
     | Debug          | -g               |
     | Release        | -O3 -DNDEBUG     |
     | RelWithDebInfo | -O2 -g -DNDEBUG  |
     | MinSizeRel     | -Os -DNDEBUG     |
     | Fedora         | <All Fedora build system options> |

  If no `CMAKE_BUILD_TYPE` value is supplied, the value of `Debug`
  will be used.

  To get additional output during the builds, you can add the
  `-DVERBOSE=ON` argument to the initial cmake command.

## Control Number of Jobs

  If building with 'Unix Makefiles', use one of the following variants
  of the cmake build command:

  ```
  $ cmake --build mybuild -j <n>
  $ CMAKE_BUILD_PARALLEL_LEVEL=<n> cmake --build mybuild
  ```

  If building with Ninja, it has a builtin algorithm that sets the
  number of parallel builds to the number of available CPUs.  There is
  some additional logic in the MythTV CMakeLists file to try and
  prevent builds from overwhelming a Rpi3.

## Android Cross Compile

  To compile a 64-bit arm Android version of MythTV, issue the
  following commands.

  ```
  $ cmake --preset android-v8-qt5
  $ cmake --build build-android-v8-qt5
  ```

  To build a 32bit arm version of android, use the following commands.

  ```
  $ cmake --preset android-v7-qt5
  $ cmake --build build-android-v7-qt5
  ```

  To see the other available presets, use this command.
  ```
  $ cmake --list-presets
  ```

  These builds will download/compile/install a much larger set of
  dependencies than for native builds, and wil then compile/install
  MythTV, and then compile/install the plugins.  Once all this is
  finished, it will create an APK file that can be uploaded to an
  android device.  This apk can be found in the build directory.  You
  do not need to specify the CMAKE_INSTALL_DIRECTORY for an android
  build as nothing is installed.

  The external libraries that are downloaded and compiled will be
  installed the directory libsinstall-<preset> and the mythtv
  components will be installed into the directory build-<preset>.
  (There should be no need to modify these directory locations, but
  they could be change by setting values into the LIBS_INSTALL_PREFIX
  and CMAKE_INSTALL_PREFIX directories.)  When you delete the build
  directory, the compiled external libraries will survive in the
  libsinstall-<preset> directory.  If you need to perform a complete
  rebuild, you will need to delete both the build-<preset> and
  libsinstall-<preset> directories.

  To set the [android
  architecture](https://cmake.org/cmake/help/latest/variable/CMAKE_ANDROID_ARCH_ABI.html)
  for the build, add the `-DCMAKE_ANDROID_ARCH_ABI=XXX` argument to
  the initial cmake command..  Valid architectures are `armeabi-v7a`,
  `arm64-v8a`, `x86`, and `x86_64`. The default build is for the
  `armeabi-v7a` architecture. For backward compatibility with the
  existing android build system, the `-DARM64=ON` argument is
  shorthand for `-DCMAKE_ANDROID_ARCH_ABI=arm64-v8a`.  You can also
  use the `-DSDK=<number>` argument as shorthand for specifying the
  SDK version, although it may be simpler to specify this in an
  overrides file.

## Windows Cross Compile

  To compile an Windows version of MythTV, issue the following
  commands.

  ```
  $ cmake --preset mingw-qt5
  $ cmake --build build-mingw-qt5
  ```

  As with android, these builds will download/compile/install a much
  larger set of dependencies thatn for native builds, and wil then
  compile/install MythTV, and then compile/install the plugins.  Once
  all this is finished, it will create the file
  <build>/MythTV_Windows.zip that can be copied to a windows system.
  You do not need to specify the CMAKE_INSTALL_DIRECTORY for an
  windows build as nothing is installed.

  Also as with android, the external libraries that are downloaded and
  compiled will be installed the directory
  <builddir>/tmp-libsinstall-win-qt5 and the mythtv components will
  be installed into the directory <builddir>/tmp-install-win-qt5.
  (There should be no need to modify these directory locations, but
  they could be change by setting values into the LIBS_INSTALL_PREFIX
  and CMAKE_INSTALL_PREFIX directories.)  Because these directories
  are located underneath the build directory, they will both be
  deleted when the build directory is deleted. If you want the
  libraries to survive deleting the build directory, you may place the
  LIBS_INSTALL_DIRECTORY anywhere you want.  The easiest solution is
  to use one of the presets ending with "-libs" that will place the
  library directory next to the build directory instead of underneath
  the build directory.

## Building subsets of MythTV

### Building just the external libraries

  CMake will create a special target named `external_libs` that can be
  used to compile only the external libraries that are downloaded and
  build for an android or windows compile.  It shouldn't be necessary
  to invoke this target directly.

  ```
  $ cmake --build build --target external_libs
  ```

### Building just the libraries

  CMake will create a special target named `libs` that can be used to
  compile only the external libraries that are downloaded and build
  for an android or windows compile, plus the external libraries that
  are embedded in the MythTV source code.  It shouldn't be necessary
  to invoke this target directly.

  ```
  $ cmake --build build --target libs
  ```

## Additional targets

  To run the test suites, use the following command:

  ```
  $ cmake --build build -t MythTV-tests
  ```

  Tests are only built for native compiles. Cross compiling for
  windows or android will not create the test suite.

  To build the documentation, use the following command:

  ```
  $ cmake --build build -t MythTV-docs
  ```

  To build the tags files for ctags/etags/gtags, use the following
  commands:

  ```
  $ cmake --build build -t ctags
  $ cmake --build build -t etags
  $ cmake --build build -t gtags
  ```

  To run clang-tidy or cppcheck over the sources, use the following
  commands:

  ```
  $ cmake --build build -t clang-tidy
  $ cmake --build build -t cppcheck
  ```

  Its also possible to run these two tools as part of the normal build
  process by setting the variables CMAKE_CXX_CPPCHECK
  CMAKE_CXX_CLANG_TIDY, althought this will significantly slow down a
  full build (because some of the clang tidy checks are expensive.)

## Options {#options}

  The file cmake/MythOptions.cmake contains default values for all of
  the options that can be enabled/disabled/modified as part off the
  build process.  This is a project wide file and should not be
  modified locally.  All entries in this file that use the option()
  command are booleans, and should be set to "ON" or "OFF".  The rest
  of the entries are strings.

  The build also loads two per-user options files (by default named
  `.config/MythTV/BuildOverridesPre.cmake` and
  `.config/MythTV/BuildOverridesPost.cmake`) so that you can
  permanently disable an option for all your builds, or adjust other
  aspects of the build.  For example, creating the file
  `.config/MythTV/BuildOverridesPost.cmake` with the following
  contents will disable the python bindings for all builds, and set
  the location of your Android ndk.

  ```
  set(ENABLE_BINDINGS_PYTHON OFF)
  set(CMAKE_ANDROID_NDK $ENV{HOME}/Android/Sdk/ndk/26.1.10909125)
  ```

  These file contain cmake commands, so you can add programming to
  make changes specific to any variable provided by cmake.  For
  example, to only disable the plugins when installing in a directory
  that contains the words "some-string", you would include the
  following in the BuildOverridesPost file.

  ```
  if(CMAKE_INSTALL_PREFIX MATCHES "some-string")
    set(MYTH_BUILD_PLUGINS OFF)
  endif()
  ```

## Notes

  1) For native builds, the minimum supported version of CMake is
     3.20.  This means that Ubuntu versions older than 22.04 and
     Debian versions older than 12/Bookworm aren't supported.

  2) For cross compiled Android builds, the minimum supported version
     of CMake is 3.21.  This means that in addition to the previous
     restrictions, all current RHEL and SuSe 15.x versions aren't
     supported.

  3) For cross compiled Android builds, the minimum supported version
     of the Android NDK is 23.

  4) To get verbose output from a build, set the environment variable
     VERBOSE (to anything) when issuing the `cmake -S . -B mybuild -G
     Ninja` command.

## References

  * [CMake Documentation](https://cmake.org/cmake/help/latest/index.html)
  * [Professional CMake: A Practical Guide](https://crascit.com/professional-cmake)

## PDF/HTML Versions

  To create pdf and html versions of this file, use the command:

  ```
  pandoc README.CMake.md -o README.CMake.html
  pandoc README.CMake.md -o README.CMake.pdf
  ```
