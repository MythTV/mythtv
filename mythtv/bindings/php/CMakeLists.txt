#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# This directory is only processed when USING_BINDINGS_PHP is set.
#
if(NOT USING_BINDINGS_PHP)
  return()
endif()

#
# If any PHP processing needs to be done in the future, add COMMAND statements
# to this custom tommand.  This trio of cmake statements ensures that this
# process occurs once and only once.
#
add_custom_command(
  OUTPUT php-bindings.stamp # Add COMMAND here to do whatever is necessary.
  COMMAND touch php-bindings.stamp)
add_custom_target(php_bindings DEPENDS php-bindings.stamp)
add_dependencies(bindings php_bindings)

install(
  FILES MythBackend.php
        MythBase.php
        MythFrontend.php
        MythTV.php
        MythTVChannel.php
        MythTVProgram.php
        MythTVRecording.php
        MythTVStorageGroup.php
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/bindings/php)
