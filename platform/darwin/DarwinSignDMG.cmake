#
# Copyright (C) 2022-2024 John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#
if(NOT APPLE)
  return()
endif()

if(DARWIN_NOTARIZATION_KEYCHAIN STREQUAL "")
  return()
endif()

if(CPACK_APP_SIGN_FAILURE OR CPACK_APP_NOTARIZATION_FAILURE)
  return()
endif()

set(DMG_FILE ${CPACK_TEMPORARY_PACKAGE_FILE_NAME})

message(STATUS "Signing the DMG Distribution File")
execute_process(
  COMMAND zsh -c "codesign --force --deep --sign \"${CPACK_DARWIN_SIGNING_ID}\" ${DMG_FILE}"
  RESULT_VARIABLE CS_OUT)

if(NOT CS_OUT EQUAL 0)
  set(CPACK_DMG_SIGN_FAILURE ON)
  message(FATAL_ERROR "DMG Code Signing Failure")
  return()
endif()

message(STATUS "Notarizing the DMG Distribution File")
execute_process(
  COMMAND zsh -c "${CMAKE_CURRENT_LIST_DIR}/notarizeFiles.zsh ${DMG_FILE} \"${CPACK_DARWIN_NOTARIZATION_KEYCHAIN}\""
  RESULT_VARIABLE NOTA_OUT)

if(NOT NOTA_OUT EQUAL 0)
  set(CPACK_DMG_NOTARIZATION_FAILURE ON)
  message(FATAL_ERROR "DMG Notarization Failure")
endif()