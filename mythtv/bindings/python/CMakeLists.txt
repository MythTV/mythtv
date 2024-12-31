#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# This directory is only processed when USING_BINDINGS_PYTHON is set.
#
if(NOT USING_BINDINGS_PYTHON)
  return()
endif()

#
# Building out of tree.  Copy everything to the binary directory so that the
# python install scripts will worlk properly.
#
file(
  COPY MythTV
       pyproject.toml
       scripts
       setup.py
       tmdb3
       ttvdbv4
       tvmaze
  DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if("pip" IN_LIST Python3Modules_COMPONENTS_FOUND
   AND "wheel" IN_LIST Python3Modules_COMPONENTS_FOUND)
  message(STATUS "Using python pip install method")
  set(USE_PYTHON_PIP TRUE)
else()
  if(NOT "pip" IN_LIST Python3Modules_COMPONENTS_FOUND)
    list(APPEND _pi_missing "pip")
  endif()
  if(NOT "wheel" IN_LIST Python3Modules_COMPONENTS_FOUND)
    list(APPEND _pi_missing "wheel")
  endif()
  message(STATUS "Python pip install not available (missing module(s): ${_pi_missing})")
  message(STATUS "Using python setup.py install method")
endif()

#
# Variables used to build and install
#
set(PIP_OPTIONS -q --no-build-isolation --no-cache-dir --no-index
                --disable-pip-version-check --no-deps)
set(WHEEL_DIR dist)
if(NOT MYTH_BINDINGS_INSTALL_ROOT STREQUAL "")
  set(ROOT_FLAGS --root=${MYTH_BINDINGS_INSTALL_ROOT})
elseif(NOT USE_PYTHON_PIP)
  set(ROOT_FLAGS --root=/tmp)
endif()

if(NOT CMAKE_INSTALL_PREFIX MATCHES "(/usr|/usr/local)")
  set(PREFIX_FLAGS --prefix=${CMAKE_INSTALL_PREFIX})
endif()

#
# Install the MythTV version where the python bindings can find it.
#
set(MYTHTV_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
configure_file(setup.cfg.in setup.cfg @ONLY)
configure_file(MythTV/_versions.py.in MythTV/_versions.py @ONLY)
configure_file(MythTV/services_api/mythversions.py.in
               MythTV/services_api/mythversions.py @ONLY)

if(USE_PYTHON_PIP)
  #
  # This trio of cmake statements ensures that this process occurs once and only
  # once.
  #
  add_custom_command(
    OUTPUT python-bindings.stamp
    COMMAND ${Python3_EXECUTABLE} -m pip wheel ${PIP_OPTIONS} --wheel-dir
            ${PROJECT_BINARY_DIR}/${WHEEL_DIR} .
    COMMAND touch python-bindings.stamp
    DEPENDS setup.py)
  add_custom_target(python_bindings DEPENDS python-bindings.stamp)
  add_dependencies(bindings python_bindings)

  #
  # Call the python installer to do the install.
  #
  install(
    CODE "execute_process(COMMAND ${Python3_EXECUTABLE} -m pip install ${ROOT_FLAGS}
                                ${PREFIX_FLAGS} ${PIP_OPTIONS}
                                 --find-links ${PROJECT_BINARY_DIR}/${WHEEL_DIR} MythTV)"
  )
else()
  #
  # Use the traditional python setup.py method to generate the build files. This
  # trio of cmake statements ensures that this process occurs once and only
  # once.
  #
  add_custom_command(
    OUTPUT python-bindings.stamp
    COMMAND ${Python3_EXECUTABLE} setup.py build
    COMMAND touch python-bindings.stamp
    DEPENDS setup.py)
  add_custom_target(python_bindings DEPENDS python-bindings.stamp)
  add_dependencies(bindings python_bindings)

  #
  # Call the python installer to do the install.
  #
  install(CODE "execute_process(COMMAND ${Python3_EXECUTABLE} setup.py install
                                --skip-build ${ROOT_FLAGS} ${PREFIX_FLAGS}
                              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
endif()

#
# Prevent namespace pollution
#
unset(PIP_OPTIONS)
unset(WHEEL_DIR)
unset(ROOT_FALGS)
unset(PREFIX_FLAGS)
unset(MYTHTV_INSTALL_PREFIX)
