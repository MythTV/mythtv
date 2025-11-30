#
# Copyright (C) 2025 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(EXISTS /usr/X11R6)
  list(APPEND CMAKE_MODULE_PATH /usr/X11R6/lib/pkgconfig)
  set(QTGUI_CONFIG_EXTRA /usr/local/include/X11)
else()
  message(FATAL_ERROR "X11 must be installed.")
endif()

# Tell MythFindQt.cmake where to find Qt.
set(Qt5_DIR /usr/local/lib/qt5/cmake/Qt5)
