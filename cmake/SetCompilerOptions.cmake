#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Load needed functions
#
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

#
# Symbol Visibility
#
if(ENABLE_HIDE_SYMBOLS)
  set(CMAKE_C_VISIBILITY_PRESET hidden)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
  list(APPEND MYTHTV_BUILD_CONFIG_LIST "using_hidesyms")
endif()

#
# Always use position independent code.
#
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(CMAKE_BUILD_TYPE MATCHES "([A-Za-z])(.*)")
  string(TOUPPER ${CMAKE_MATCH_1} _INIT)
  string(TOLOWER ${CMAKE_MATCH_2} _REST)
  include(SetCompilerOptions${_INIT}${_REST} OPTIONAL)
endif()

#
# Always used flags
#
list(
  APPEND
  CFLAGS
  -fdiagnostics-color=auto
  -fno-math-errno
  -fno-signed-zeros
  -fno-tree-vectorize
  -mstack-alignment=16
  -Wall
  -Wextra
  -Wduplicated-branches
  -Wduplicated-cond
  -Werror=format-security
  -Werror=implicit-function-declaration
  -Werror=return-type
  -Werror=vla
  -Wjump-misses-init
  -Wlogical-op
  -Wnull-dereference
  -Wpointer-arith
  -Wwrite-strings)

list(
  APPEND
  CXXFLAGS
  -faligned-new
  -fdiagnostics-color=auto
  -fno-math-errno
  -fno-signed-zeros
  -fno-tree-vectorize
  -funit-at-a-time
  -mstack-alignment=16
  -Qunused-arguments
  -Wall
  -Wextra
  # -Wdouble-promotion
  -Wduplicated-cond
  -Werror=format-security
  -Werror=implicit-function-declaration
  -Werror=return-type
  -Werror=vla
  -Wimplicit-fallthrough
  -Wjump-misses-init
  -Wlogical-op
  -Wmissing-declarations
  -Wnull-dereference
  -Woverloaded-virtual
  -Wpointer-arith
  -Wredundant-decls
  -Wsuggest-override
  -Wundef
  # -Wall flags to disable
  -Wno-unknown-pragmas # gcc doesn't recognize clang pragmas
)
if(NOT ANDROID)
  list(APPEND CXXFLAGS "-Wshadow")
  list(APPEND CFLAGS "-Wshadow")
endif()

#
# Add compiler specific flags.
#
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  list(APPEND CXXFLAGS -Wzero-as-null-pointer-constant)

  # This warning flag isn't enabled yet because it will require a large number
  # of changes to the code to eliminate all the warnings.
  #
  # list(APPEND CXXFLAGS -Wold-style-cast)

  # This warning flag can't be enabled because the Qt5 moc compiler produces
  # files that contain "useless" casts.
  #
  # list(APPEND CXXFLAGS -Wuseless-cast)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

  # The constant-logical-operand only gives use false positives for constant
  # logical operands meant to be optimized away.
  list(APPEND CXXFLAGS -Wno-constant-logical-operand)

  # Unfortunately clang's unused-value warning is smart enough to notice that
  # ZMQ_ASSERT expressions don't use the values unless it is a debug build. So
  # lets rely on other compilers to report on unused values.
  list(APPEND CXXFLAGS -Wno-unused-value)

  # Clang on FreeBSD doesn't ignore warnings in system headers. A trivial test
  # shows that Clang defaults /usr/local/include to a system directory, but
  # somehow this gets messed up in MythTV builds.
  #
  # Clang on MacOSX also doesn't ignore warnings in system headers.
  if(NOT CMAKE_SYSTEM_NAME MATCHES "(FreeBSD|Darwin)")
    list(APPEND CXXFLAGS -Wzero-as-null-pointer-constant)
  endif()

endif()

#
# Check for Interprocedural Optimization, aka Link Time Optimization.
#
include(CheckIPOSupported)
check_ipo_supported(RESULT has_ipo)
if(ENABLE_LTO AND has_ipo AND NOT CMAKE_CROSSCOMPILING)
  message(STATUS "Enabling link-time optimization.")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

#
# Now test each flag and see if its valid.
#
get_cmake_property(_ENABLED_LANGUAGES ENABLED_LANGUAGES)
list(FIND _ENABLED_LANGUAGES "C" _ENABLED_C)
if(NOT _ENABLED_C EQUAL -1)
  foreach(_FLAG IN LISTS CFLAGS)
    string(SUBSTRING ${_FLAG} 1 -1 _NAME)
    check_c_compiler_flag("${_FLAG}" HAVE_C_${_NAME})
    if(HAVE_C_${_NAME})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_FLAG}")
    endif()
  endforeach()

  foreach(_FLAG IN LISTS LFLAGS)
    string(SUBSTRING ${_FLAG} 1 -1 _NAME)
    check_linker_flag(C "${_FLAG}" HAVE_LINKER_${_NAME})
    if(HAVE_LINKER_${_NAME})
      set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${_FLAG}")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_FLAG}")
      set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${_FLAG}")
    endif()
  endforeach()
endif()

list(FIND _ENABLED_LANGUAGES "CXX" _ENABLED_CXX)
if(NOT _ENABLED_CXX EQUAL -1)
  foreach(_FLAG IN LISTS CXXFLAGS)
    string(SUBSTRING ${_FLAG} 1 -1 _NAME)
    check_cxx_compiler_flag("${_FLAG}" HAVE_CXX_${_NAME})
    if(HAVE_CXX_${_NAME})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_FLAG}")
    endif()
  endforeach()

  foreach(_FLAG IN LISTS LFLAGS)
    string(SUBSTRING ${_FLAG} 1 -1 _NAME)
    check_linker_flag(CXX "${_FLAG}" HAVE_LINKER_${_NAME})
    if(HAVE_LINKER_${_NAME})
      set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${_FLAG}")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_FLAG}")
      set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${_FLAG}")
    endif()
  endforeach()
endif()

list(REMOVE_DUPLICATES CMAKE_C_FLAGS)
list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS)
list(REMOVE_DUPLICATES CMAKE_MODULE_LINKER_FLAGS)
list(REMOVE_DUPLICATES CMAKE_SHARED_LINKER_FLAGS)
list(REMOVE_DUPLICATES CMAKE_STATIC_LINKER_FLAGS)

unset(CFLAGS)
unset(CXXFLAGS)
unset(LFLAGS)
