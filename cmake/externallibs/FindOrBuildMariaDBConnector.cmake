#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# MariaDB Connector
#
# cmake, C source only
#
function(find_or_build_maria_db_connector)
  pkg_check_modules(MARIADB "libmariadb" QUIET IMPORTED_TARGET)
  if(MARIADB_FOUND)
    message(
      STATUS "Found mariadb ${MARIADB_VERSION} at ${MARIADB_LINK_LIBRARIES}")
    add_library(mariadb_c ALIAS PkgConfig::MARIADB)
    return()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET external_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  set(MARIADB_C_VERSION "3.3.5")
  set(MARIADB_C_PREFIX "mariadb-connector-c-${MARIADB_C_VERSION}")
  set(MARIADB_C_2.3.7_SHA256
      "94f9582da738809ae1d9f1813185165ec7c8caf9195bdd04e511f6bdcb883f8e")
  set(MARIADB_C_3.3.5_SHA256
      "ca72eb26f6db2befa77e48ff966f71bcd3cb44b33bd8bbb810b65e6d011c1e5c")
  ExternalProject_Add(
    mariadb-connector
    DOWNLOAD_DIR ${TARBALL_DIR}
    URL https://downloads.mariadb.com/Connectors/c/connector-c-${MARIADB_C_VERSION}/${MARIADB_C_PREFIX}-src.tar.gz
    URL_HASH SHA256=${MARIADB_C_${MARIADB_C_VERSION}_SHA256}
    PATCH_COMMAND patch -p1 <
                  ${PROJECT_SOURCE_DIR}/patches/${MARIADB_C_PREFIX}.patch
    CMAKE_ARGS --no-warn-unused-cli
               ${CMDLINE_ARGS_LIBS}
               ${PLATFORM_ARGS}
               -DBUILD_SHARED_LIBS:BOOL=ON
               -DWITH_SSL:BOOL=OFF
               -DWITH_UNIT_TESTS:BOOL=OFF
               -DWITH_EXTERNAL_ZLIB:BOOL=ON
    CMAKE_CACHE_ARGS
      -DCMAKE_FIND_ROOT_PATH:STRING=${CMAKE_FIND_ROOT_PATH}
      -DCMAKE_JOB_POOL_COMPILE:STRING=compile
      -DCMAKE_JOB_POOL_LINK:STRING=link
      -DCMAKE_JOB_POOLS:STRING=${CMAKE_JOB_POOLS}
      -DPKG_CONFIG_LIBDIR:STRING=${PKG_CONFIG_LIBDIR}
      -DPKG_CONFIG_PATH:STRING=${PKG_CONFIG_PATH}
    DEPENDS ${after_libs})

  if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    ExternalProject_Add_Step(
      mariadb-connector fixliblib
      DEPENDEES install
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${LIBS_INSTALL_PREFIX}/lib/mariadb/liblibmariadb.dll.a
        ${LIBS_INSTALL_PREFIX}/lib/mariadb/libmariadb.dll.a
      BYPRODUCTS ${LIBS_INSTALL_PREFIX}/lib/mariadb/libmariadb.dll.a)
  endif()

  add_dependencies(external_libs mariadb-connector)

  message(STATUS "Will build mariadb-c-connector (${MARIADB_C_VERSION})")
endfunction()
