#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# This script is always processed.
#

#
# The cmake documentation strongly recommands against globbing source file
# names, thus this explicit list of filenames.
#
set(PYFILES mbutils.py)
unset(PROCESSED_FILES)
fix_shebang_install_python("metadata/Music" PYFILES)

# This custom target dependes on all of the files generated by the
# fix_shebang_install_python macro, which is what causes those commands to
# actually be executed.
add_custom_target(metadata_Music ALL DEPENDS ${PROCESSED_FILES})

#
# The remaining files can be easily installed without any processing
#
install(
  DIRECTORY discid musicbrainzngs
  DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/metadata/Music
  PATTERN "\.gitignore" EXCLUDE)

#
# Now handle the lyrics directory
#
add_subdirectory(lyrics)
