cmake_minimum_required( VERSION 3.5.0 )

project(exiv2
    VERSION 0.99.0.0   # Fake version number to indicate that the next version will be 1.0
    LANGUAGES CXX C
)

include(cmake/mainSetup.cmake  REQUIRED)
include(CTest)

include(cmake/gcovr.cmake REQUIRED)

# options and their default values
option( BUILD_SHARED_LIBS             "Build exiv2lib as a shared library"                    ON  )
option( EXIV2_ENABLE_XMP              "Build with XMP metadata support"                       ON  )
option( EXIV2_ENABLE_EXTERNAL_XMP     "Use external version of XMP"                           OFF )
option( EXIV2_ENABLE_PNG              "Build with png support (requires libz)"                ON  )
option( EXIV2_ENABLE_NLS              "Build native language support (requires gettext)"      OFF )
option( EXIV2_ENABLE_PRINTUCS2        "Build with Printucs2"                                  ON  )
option( EXIV2_ENABLE_LENSDATA         "Build including lens data"                             OFF )
option( EXIV2_ENABLE_DYNAMIC_RUNTIME  "Use dynamic runtime (used for static libs)"            ON )
option( EXIV2_ENABLE_WIN_UNICODE      "Use Unicode paths (wstring) on Windows"                OFF )
option( EXIV2_ENABLE_WEBREADY         "Build webready support into library"                   OFF )
option( EXIV2_ENABLE_CURL             "USE Libcurl for HttpIo (WEBREADY)"                     OFF )

option( EXIV2_BUILD_SAMPLES           "Build sample applications"                             OFF )
option( EXIV2_BUILD_EXIV2_COMMAND     "Build exiv2 command-line executable"                   OFF )
option( EXIV2_BUILD_UNIT_TESTS        "Build unit tests"                                      OFF )
option( EXIV2_BUILD_FUZZ_TESTS        "Build fuzz tests (libFuzzer)"                          OFF )
option( EXIV2_BUILD_DOC               "Add 'doc' target to generate documentation"            OFF )

# Only intended to be used by Exiv2 developers/contributors
option( EXIV2_TEAM_EXTRA_WARNINGS     "Add more sanity checks using compiler flags"           OFF )
option( EXIV2_TEAM_WARNINGS_AS_ERRORS "Treat warnings as errors"                              OFF )
option( EXIV2_TEAM_USE_SANITIZERS     "Enable ASAN and UBSAN when available"                  OFF )
option( EXIV2_TEAM_PACKAGING          "Additional stuff for generating packages"              OFF )
set(EXTRA_COMPILE_FLAGS " ")

mark_as_advanced(
    EXIV2_TEAM_EXTRA_WARNINGS
    EXIV2_TEAM_WARNINGS_AS_ERRORS
    EXIV2_ENABLE_EXTERNAL_XMP
    EXTRA_COMPILE_FLAGS
    EXIV2_TEAM_USE_SANITIZERS
)

option( BUILD_WITH_CCACHE             "Use ccache to speed up compilations"                   ON  )
option( BUILD_WITH_COVERAGE           "Add compiler flags to generate coverage stats"         OFF )

set( PACKAGE_BUGREPORT                "http://github.com/exiv2/exiv2" )
set( PACKAGE_URL                      "http://exiv2.dyndns.org")
set( PROJECT_DESCRIPTION              "Exif and IPTC metadata library and tools")

if ( EXIV2_ENABLE_EXTERNAL_XMP )
    set(EXIV2_ENABLE_XMP OFF)
endif()

if( EXIV2_BUILD_UNIT_TESTS )
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON) # Requires CMake 3.3.3
endif()

include(cmake/findDependencies.cmake   REQUIRED)
include(cmake/compilerFlags.cmake      REQUIRED)
include(cmake/generateConfigFile.cmake REQUIRED)

if (EXIV2_BUILD_DOC)
    include(cmake/generateDoc.cmake        REQUIRED)
    generate_documentation("${PROJECT_SOURCE_DIR}/cmake/Doxyfile.in")
endif()


include_directories(${CMAKE_BINARY_DIR})  # Make the exv_conf.h file visible for the full project

if( EXIV2_ENABLE_XMP )
    add_subdirectory( xmpsdk )
endif()

include(cmake/compilerFlagsExiv2.cmake REQUIRED)

add_subdirectory( include )
add_subdirectory( src )

if( EXIV2_BUILD_UNIT_TESTS )
    add_subdirectory ( unitTests )
endif()

if( EXIV2_BUILD_FUZZ_TESTS)
    if ((NOT COMPILER_IS_CLANG) OR (NOT EXIV2_TEAM_USE_SANITIZERS))
        message(FATAL_ERROR "You need to build with Clang and sanitizers for the fuzzers to work. "
                "Use Clang and -DEXIV2_TEAM_USE_SANITIZERS=ON")
    endif()
    add_subdirectory ( fuzz )
endif()

if( EXIV2_BUILD_SAMPLES )
    ##
    # tests
    add_custom_target(tests
        COMMAND env EXIV2_BINDIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" make test
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/test"
    )
    add_subdirectory( samples )
    get_directory_property(SAMPLES DIRECTORY samples DEFINITION APPLICATIONS)
    add_dependencies(tests exiv2lib exiv2 ${SAMPLES})
endif()

if( EXIV2_ENABLE_NLS )
    add_subdirectory( po )
endif()

if (EXIV2_TEAM_PACKAGING)
    include(cmake/packaging.cmake)
endif()

configure_file(cmake/exiv2.pc.in mythexiv2.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/mythexiv2.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# ******************************************************************************
# Man page
#install( FILES ${PROJECT_SOURCE_DIR}/man/man1/exiv2.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 )


include(cmake/printSummary.cmake)
