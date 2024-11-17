#
# Copyright (C) 2022-2024 John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#
if(NOT APPLE)
  return()
endif()

# CPACK overwrites the previously generated Info.plist, so it needs to be corrected
set(APP_FILE ${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGE_NAME}.app)

if(DARWIN_SIGNING_ID STREQUAL "")
  return()
endif()

message(STATUS "Signing the Application")
execute_process(
  COMMAND zsh -c "${CMAKE_CURRENT_LIST_DIR}/codesignApp.zsh ${APP_FILE} ${CPACK_ENTITLEMENTS_PLIST} \"${CPACK_DARWIN_SIGNING_ID}\""
  RESULT_VARIABLE CS_OUT)

if(NOT CS_OUT EQUAL 0)
  set(CPACK_APP_SIGN_FAILURE ON)
  message(FATAL_ERROR "App Code Signing Failure")
  return()
endif()

if(NOT DARWIN_NOTARIZATION_KEYCHAIN STREQUAL "")
  message(STATUS "Notarizing the Application")
  execute_process(
    COMMAND zsh -c "${CMAKE_CURRENT_LIST_DIR}/notarizeFiles.zsh ${APP_FILE} ${CPACK_DARWIN_NOTARIZATION_KEYCHAIN}"
    RESULT_VARIABLE NOTA_OUT)
endif()

if(NOT NOTA_OUT EQUAL 0)
  set(CPACK_APP_NOTARIZATION_FAILURE ON)
  message(FATAL_ERROR "App Notarization Signing Failure")
endif()