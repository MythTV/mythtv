#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# Does this system have pkgconf or pkg-config.  Both use the executable name
# pkg-config.
#
# https://gitlab.freedesktop.org/pkg-config/pkg-config (version numbers up to
# 0.29.2) is used on debian < 12 and ubuntu < 23.04.
#
# https://github.com/pkgconf/pkgconf (version numbers from 1.0 and up) is used
# on debian >= 12, ubuntu >= 23.04, fedora, SuSe, Arch.
#
# Only pkgconf has the --env-only flag
execute_process(
  COMMAND ${PKG_CONFIG_EXECUTABLE} --version
  OUTPUT_VARIABLE PKG_CONFIG_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

# If cross compiling, make sure that pkg-config only looks for host system
# libraries, and doesn't find any build system libraries.
if(CMAKE_CROSSCOMPILING)
  if(PKG_CONFIG_VERSION VERSION_GREATER_EQUAL 1.0)
    if(${CMAKE_VERSION} VERSION_LESS 3.22)
      set(PKG_CONFIG "pkg-config" "--env-only")
    else()
      set(PKG_CONFIG_ARGN "--env-only")
    endif()
  endif()
endif()

# The SetSearchPaths file and the customization files have set up the
# PKG_CONFIG_XXX variables.  Now export them into the environment so that they
# can be found by the pkg-config executable.  All calls to pkg_check_modules in
# this project will end up referencing these variables.
#
# Note: Any sub-project will also need to install these variables into its
# environment, so that its calls to pkg_check_modules can reference the same
# paths.  Those are going to be called from the invocation of "cmake --build"
# not this invocation of "cmake -B xxx -G yyy".
#
if(PKG_CONFIG)
  set(ENV{PKG_CONFIG} ${PKG_CONFIG})
endif()
if(PKG_CONFIG_ARGN)
  set(ENV{PKG_CONFIG_ARGN} ${PKG_CONFIG_ARGN})
endif()
if(PKG_CONFIG_LIBDIR)
  set(ENV{PKG_CONFIG_LIBDIR} "${PKG_CONFIG_LIBDIR}")
endif()
if(PKG_CONFIG_PATH)
  list(JOIN PKG_CONFIG_PATH ":" PKG_CONFIG_PATH_STR)
  set(ENV{PKG_CONFIG_PATH} ${PKG_CONFIG_PATH_STR})
endif()
