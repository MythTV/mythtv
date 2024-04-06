#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# This directory is only processed when USING_BINDINGS_PERL is set.
#
if(NOT USING_BINDINGS_PERL)
  return()
endif()

#
# Building out of tree.  Copy everything to the binary directory so that the
# perl install scripts will worlk properly.
#
file(COPY MythTV.pm IO MythTV DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

#
# Install the MythTV version where the perl bindings can find it.
#
configure_file(Makefile.PL.in Makefile.PL @ONLY)

#
# Use the traditional perl Makefile.PL method to generate the build files. This
# trio of cmake statements ensures that this process occurs once and only once.
#
add_custom_command(
  OUTPUT perl-bindings.stamp
  COMMAND ${PERL_EXECUTABLE} Makefile.PL MAKEFILE=Makefile.perl
          PREFIX="${CMAKE_INSTALL_PREFIX}" ${MYTH_PERL_CONFIG_OPTS}
  COMMAND ${MAKE_EXECUTABLE} -f Makefile.perl
  COMMAND touch perl-bindings.stamp
  DEPENDS Makefile.PL)
add_custom_target(perl_bindings DEPENDS perl-bindings.stamp)
add_dependencies(bindings perl_bindings)

#
# Call the perl installer to do the install.
#
install(
  CODE "execute_process(COMMAND ${MAKE_EXECUTABLE} -f Makefile.perl pure_install
                                PREFIX=${CMAKE_INSTALL_PREFIX}
                                PERL_INSTALL_ROOT=${MYTH_BINDINGS_INSTALL_ROOT}
                              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")

# Add additional files and directories to make clean
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
                                    "output;blib;pm_to_blib;MakefilePerl")
