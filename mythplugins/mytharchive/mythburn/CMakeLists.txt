#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

include(InstallTheme)

install(DIRECTORY encoder_profiles images intro music scripts themes
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/mythtv/mytharchive)
