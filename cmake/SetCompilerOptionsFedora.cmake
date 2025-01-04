#
# Copyright (C) 2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#
if((CMAKE_C_COMPILER_ID AND NOT CMAKE_C_COMPILER_ID STREQUAL GNU) OR
   (CMAKE_CXX_COMPILER_ID AND NOT CMAKE_CXX_COMPILER_ID STREQUAL GNU))
  message(
    FATAL_ERROR "The Fedora build type requires the use of the gcc compiler.")
endif()

set(CMAKE_C_FLAGS_FEDORA
    "-O2 -flto=auto -ffat-lto-objects -fexceptions -g -grecord-gcc-switches -pipe -Wall -Werror=format-security -Wp,-U_FORTIFY_SOURCE,-D_FORTIFY_SOURCE=3 -Wp,-D_GLIBCXX_ASSERTIONS -specs=/usr/lib/rpm/redhat/redhat-hardened-cc1 -fstack-protector-strong -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1  -m64 -march=x86-64 -mtune=generic -fasynchronous-unwind-tables -fstack-clash-protection -fcf-protection -mtls-dialect=gnu2 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer"
)
set(CMAKE_CXX_FLAGS_FEDORA
    "-O2 -flto=auto -ffat-lto-objects -fexceptions -g -grecord-gcc-switches -pipe -Wall -Werror=format-security -Wp,-U_FORTIFY_SOURCE,-D_FORTIFY_SOURCE=3 -Wp,-D_GLIBCXX_ASSERTIONS -specs=/usr/lib/rpm/redhat/redhat-hardened-cc1 -fstack-protector-strong -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1  -m64 -march=x86-64 -mtune=generic -fasynchronous-unwind-tables -fstack-clash-protection -fcf-protection -mtls-dialect=gnu2 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer"
)
set(CMAKE_EXE_LINKER_FLAGS_FEDORA
    "-Wl,-z,relro -Wl,--as-needed  -Wl,-z,pack-relative-relocs -Wl,-z,now -specs=/usr/lib/rpm/redhat/redhat-hardened-ld -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1  -Wl,--build-id=sha1"
)
set(CMAKE_MODULE_LINKER_FLAGS_FEDORA "${CMAKE_EXE_LINKER_FLAGS_FEDORA}")
set(CMAKE_SHARED_LINKER_FLAGS_FEDORA "${CMAKE_EXE_LINKER_FLAGS_FEDORA}")
