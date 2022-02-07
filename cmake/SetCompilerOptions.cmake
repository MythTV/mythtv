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

#
# Always used flags
#
list(
  APPEND
  CFLAGS
  -fno-math-errno
  -fno-signed-zeros
  -Wall
  -Wcast-qual
  -Wdeclaration-after-statement
  -Wextra
  -Wno-pointer-to-int-cast
  -Wpedantic
  -Wpointer-arith
  -Wredundant-decls
  -Wredundant-decls
  -Wstrict-prototypes
  -Wundef
  -Wwrite-strings)

list(
  APPEND
  CXXFLAGS
  -fno-math-errno
  -fno-signed-zeros
  -Wall
  -Wextra
  -Wpointer-arith
  -Wundef)

if(ENABLE_LTO AND NOT CMAKE_CROSSCOMPILING)
  list(APPEND CFLAGS -flto)
  list(APPEND CXXFLAGS -flto)
  list(APPEND LFLAGS -flto)
endif()

if(NOT ANDROID)
  list(APPEND CXXFLAGS "-Wshadow")
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  list(APPEND CFLAGS -Wmissing-prototypes -Werror=missing-prototypes)
endif()

#
# Add compiler specific flags.
#
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  list(
    APPEND
    CFLAGS
    -fdiagnostics-color=auto
    -fno-tree-vectorize
    -Wdouble-promotion
    -Wduplicated-cond
    -Wduplicated-branches
    -Werror=format-security
    -Werror=implicit-function-declaration
    -Werror=return-type
    -Werror=vla
    -Wjump-misses-init
    -Wlogical-op
    -Wnull-dereference)

  list(
    APPEND
    CXXFLAGS
    -faligned-new
    -funit-at-a-time
    -Wdouble-promotion
    -Wduplicated-cond
    -Wlogical-op
    -Wmissing-declarations
    -Wnull-dereference
    -Woverloaded-virtual
    -Wzero-as-null-pointer-constant)

  # This warning flag isn't enabled yet because it will require a large number
  # of changes to the code to eliminate all the warnings.
  # add_compile_options($<${gnu_cxx}:$<BUILD_INTERFACE:-Wold-style-cast>>)

  # This warning flag can't be enabled because the Qt5 moc compiler produces
  # files that contain "useless" casts.
  # add_compile_options($<${gnu_cxx}:$<BUILD_INTERFACE:-Wuseless-cast>>)

  # The Q_OBJECT macro gets included in every subclass that is based on QOBject,
  # and has a couple of functions marked as "virtual" instead of "override".
  # Apparently earlier version of GCC didn't warn about this because the errors
  # were in the /usr/include directory. GCC11 does print these warnings. A ton
  # of them. Disable this warning to make it easier to find real errors.
  if(CXX_COMPILER_VERSION VERSION_LESS 11.0.0)
    list(APPEND CXXFLAGS -Wsuggest-override)
  endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

  list(
    APPEND
    CFLAGS
    -mllvm
    -mstack-alignment=16
    -Qunused-arguments
    -Werror=implicit-function-declaration
    -Werror=return-type
    -Wimplicit-fallthrough)

  # The constant-logical-operand only gives use false positives for constant
  # logical operands meant to be optimized away.
  list(APPEND CXXFLAGS -Wno-constant-logical-operand)

  # Unfortunately clang's unused-value warning is smart enough to notice that
  # ZMQ_ASSERT expressions don't use the values unless it is a debug build. So
  # lets rely on other compilers to report on unused values.
  list(APPEND CXXFLAGS -Wno-unused-value)

  # clang complains about every unused -I unless you pass it this.
  list(APPEND CXXFLAGS -Qunused-arguments)

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
# Now test each flag and see if its valid.
#
get_cmake_property(_ENABLED_LANGUAGES ENABLED_LANGUAGES)
list(FIND _ENABLED_LANGUAGES "C" _ENABLED_C)
if(NOT _ENABLED_C EQUAL -1)
  foreach(_FLAG IN LISTS CFLAGS)
    string(SUBSTRING ${_FLAG} 1 -1 _NAME)
    check_c_compiler_flag("${_FLAG}" HAVE_C_${_NAME})
    if(HAVE_C_${_FLAG}_NAME)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_FLAG}}")
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
    if(HAVE_CXX_${_FLAG}_NAME)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_FLAG}}")
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
