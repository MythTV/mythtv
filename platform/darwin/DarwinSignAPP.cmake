#
# Copyright (C) 2022-2024 John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#
if(NOT APPLE)
  return()
endif()

if(DARWIN_SIGNING_ID STREQUAL "")
  return()
endif()

set(APP_BUNDLE_PATH "${CPACK_TEMPORARY_DIRECTORY}/${CPACK_PACKAGING_INSTALL_PREFIX}")

message(STATUS "Signing the Application")
execute_process(
  COMMAND zsh -c "${CODESIGN_SCRIPT} ${APP_BUNDLE_PATH} ${CPACK_ENTITLEMENTS_PLIST} \"${CPACK_DARWIN_SIGNING_ID}\""
  RESULT_VARIABLE CS_OUT)

if(NOT CS_OUT EQUAL 0)
  set(CPACK_APP_SIGN_FAILURE ON)
  message(FATAL_ERROR "App Code Signing Failure")
  return()
endif()

if(NOT DARWIN_NOTARIZATION_KEYCHAIN STREQUAL "")
  message(STATUS "Notarizing the Application")
  execute_process(
    COMMAND zsh -c "${NOTARIZE_SCRIPT} ${APP_BUNDLE_PATH} ${CPACK_DARWIN_NOTARIZATION_KEYCHAIN}"
    RESULT_VARIABLE NOTA_OUT)
endif()

if(NOT NOTA_OUT EQUAL 0)
  set(CPACK_APP_NOTARIZATION_FAILURE ON)
  message(FATAL_ERROR "App Notarization Signing Failure")
endif()
