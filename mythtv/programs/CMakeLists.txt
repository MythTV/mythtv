#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(ANDROID)
  add_subdirectory(mythfrontend)
  return()
endif()

if(ENABLE_BACKEND)
  add_subdirectory(mythbackend)
  add_subdirectory(mythfilldatabase)
  add_subdirectory(mythmetadatalookup)
  add_subdirectory(mythtv-setup)
  if(NOT WIN32)
    add_subdirectory(mythexternrecorder)
    add_subdirectory(mythfilerecorder)
    add_subdirectory(scripts)
  endif()
endif()

if(ENABLE_FRONTEND)
  add_subdirectory(mythavtest)
  add_subdirectory(mythccextractor)
  add_subdirectory(mythcommflag)
  add_subdirectory(mythfrontend)
  add_subdirectory(mythjobqueue)
  add_subdirectory(mythlcdserver)
  add_subdirectory(mythmediaserver)
  add_subdirectory(mythpreviewgen)
  add_subdirectory(mythscreenwizard)
  add_subdirectory(mythshutdown)
  add_subdirectory(mythtranscode)
  add_subdirectory(mythutil)
  if(NOT WIN32)
    add_subdirectory(mythwelcome)
  endif()
endif()
