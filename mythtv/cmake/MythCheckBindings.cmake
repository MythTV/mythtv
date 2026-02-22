#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Include MythTV CMake functions
#
include(BuildConfigString)

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
  find_program(PHP_EXECUTABLE NAMES php php-8.4)
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
