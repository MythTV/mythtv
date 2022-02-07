#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT ENABLE_BDJAVA)
  return()
endif()

#
# Current versions of ant require that java_version_bdj have a value of 1.5 or
# higher. This corresponds to a minimum java version of 9.0. Should this code
# simply require that version is part of the find_package call?
#
# The FindJava.cmake package respects the JAVA_HOME variable.
find_package(Java COMPONENTS Development)
if(NOT Java_FOUND OR NOT Java_JAVAC_EXECUTABLE)
  message(
    FATAL_ERROR "BD-J requires JAVAC, but it wasn't found. Please install it.")
endif()

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(JAVA_ARCH "amd64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_32")
  set(JAVA_ARCH "i386")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm")
  set(JAVA_ARCH "arm")
endif()
message(DEBUG "JAVA_ARCH is ${JAVA_ARCH}")

if(${Java_VERSION} VERSION_GREATER_EQUAL 12.0.0)
  set(JAVA_CODE_VERSION 1.7)
elseif(${Java_VERSION} VERSION_GREATER_EQUAL 9.0.0)
  set(JAVA_CODE_VERSION 1.6)
else()
  set(JAVA_CODE_VERSION 1.4)
endif()
message(DEBUG "JAVA_CODE_VERSION is ${JAVA_CODE_VERSION}")

find_program(ANT_PATH ant)
if(NOT ANT_PATH)
  message(
    FATAL_ERROR "BD-J requires ANT, but it wasn't found. Please install it.")
endif()
message(DEBUG "ant is ${ANT_PATH}")

set(BDJ_TYPE j2se)
set(BDJAVA_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/build.xml)
set(BDJAVA_VERSIONS
    -Dversion='${BDJ_TYPE}-${BLURAY_VERSION}'
    -Djava_version_asm=${JAVA_CODE_VERSION}
    -Djava_version_bdj=${JAVA_CODE_VERSION})
set(BDJAVA_LOCATIONS -Dbuild="${CMAKE_CURRENT_SOURCE_DIR}/build"
                     -Ddist="${CMAKE_CURRENT_SOURCE_DIR}")
add_build_config(TRUE "bdjava")

#
# Create target to build java libraries, and make it a dependency of mythbluray
# to ensure it gets built.
#
add_custom_target(
  bdjava
  DEPENDS ${BDJAVA_DEPENDS}
  BYPRODUCTS libmythbluray-${BDJ_TYPE}-${BLURAY_VERSION}.jar
             libmythbluray-awt-${BDJ_TYPE}-${BLURAY_VERSION}.jar
  COMMAND ${CMAKE_COMMAND} -E echo "Build bdjava"
  COMMAND ${ANT_PATH} -f ${BDJAVA_DEPENDS} ${BDJAVA_LOCATIONS}
          -Dsrc_awt=:java-${BDJ_TYPE}:java-build-support ${BDJAVA_VERSIONS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(mythbluray bdjava)

#
# Installation section
#

install(FILES libmythbluray-${BDJ_TYPE}-${BLURAY_VERSION}.jar
              libmythbluray-awt-${BDJ_TYPE}-${BLURAY_VERSION}.jar
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/jars)
