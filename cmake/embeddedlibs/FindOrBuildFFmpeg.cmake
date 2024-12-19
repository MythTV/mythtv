#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

function(find_or_build_ffmpeg)
  if(LIBS_USE_INSTALLED)
    pkg_check_modules(LIBAVCODEC libmythavcodec QUIET IMPORTED_TARGET)
    if(TARGET PkgConfig::LIBAVCODEC)
      message(STATUS "Found FFmpeg in ${LIBAVCODEC_LIBDIR}")
      set(ProjectDepends
          ${ProjectDepends} PkgConfig::LIBAVCODEC
          PARENT_SCOPE)
      return()
    endif()
  endif()

  if(ENABLE_STRICT_BUILD_ORDER)
    get_property(
      after_libs
      TARGET embedded_libs
      PROPERTY MANUALLY_ADDED_DEPENDENCIES)
  endif()

  if(LIBS_INSTALL_FFMPEG)
    set(FFMPEG_INSTALL_PREFIX ${LIBS_INSTALL_PREFIX})
  else()
    set(FFMPEG_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
  endif()

  #
  # Always build our version of FFmpeg
  #
  list(
    PREPEND
    FF_ARGS
    --prefix=${FFMPEG_INSTALL_PREFIX}
    --libdir=${FFMPEG_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
    --arch=${CMAKE_SYSTEM_PROCESSOR}
    --cpu=generic
    --disable-demuxer=mpegtsraw
    --disable-doc
    --disable-ffplay
    --disable-htmlpages
    --disable-indev=dshow
    --disable-manpages
    --disable-podpages
    --disable-txtpages
    --enable-gpl
    --enable-pic
    --disable-stripping
    --disable-static
    --enable-shared
    )

  if(NOT LIBS_INSTALL_PREFIX STREQUAL CMAKE_INSTALL_PREFIX)
    list(APPEND FF_ARGS --extra-cflags=-I${LIBS_INSTALL_PREFIX}/include
         --extra-ldflags=-L${LIBS_INSTALL_PREFIX}/lib
         --extra-ldflags=-L${LIBS_INSTALL_PREFIX}/lib64)
  endif()

  if(ANDROID)
    list(APPEND FF_ARGS --enable-mediacodec --enable-jni)
  endif()

  #
  # Do the lame libraries already exist, or will they exist by the time FFmpeg
  # is compiled?
  #
  if(TARGET Lame::Lame)
    # Already exists.
    list(APPEND FF_ARGS --enable-libmp3lame)
  elseif(TARGET lame)
    # Will be built by the time the FFmpeg project is built.
    list(APPEND FF_ARGS --enable-libmp3lame)
  else()
    list(APPEND FF_ARGS --disable-libmp3lame)
  endif()

  #
  # Handle platform args for a native system.
  #
  if(NOT CMAKE_CROSSCOMPILING)
    string(TOLOWER ${CMAKE_SYSTEM_NAME} CMAKE_SYSTEM_NAME_LC)
    list(
      PREPEND
      FF_PLATFORM_ARGS
      "--sysinclude=/usr/include"
      "--target_os=${CMAKE_SYSTEM_NAME_LC}"
      "--disable-cross-compile"
      "--cc=${CMAKE_C_COMPILER}"
      "--cxx=${CMAKE_CXX_COMPILER}")
  endif()

  #
  # Create the project to build FFmpeg
  #
  ExternalProject_Add(
    FFmpeg
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mythtv/external/FFmpeg
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env ${_PROGS} PKG_CONFIG_PATH=${PKG_CONFIG_PATH_STR}
      ${CMAKE_CURRENT_SOURCE_DIR}/mythtv/external/FFmpeg/configure ${FF_ARGS}
      "${FF_PLATFORM_ARGS}"
      $<IF:$<BOOL:${SYSTEM_LIBBLURAY_FOUND}>,--enable-libbluray,--disable-libbluray>
      $<IF:$<TARGET_EXISTS:Fontconfig::Fontconfig::LIBXVID>,--enable-libfontconfig,--disable-libfontconfig>
      $<IF:$<TARGET_EXISTS:LibX264::LibX264>,--enable-libx264,--disable-libx264>
      $<IF:$<TARGET_EXISTS:LibX265::LibX265>,--enable-libx265,--disable-libx265>
      $<IF:$<TARGET_EXISTS:PkgConfig::LibXml2>,--enable-libxml2,--disable-libxml2>
      $<IF:$<TARGET_EXISTS:LibXvid::LIBVPX>,--enable-libvpx,--disable-libvpx>
      $<IF:$<TARGET_EXISTS:PkgConfig::DRM>,--enable-libdrm,--disable-libdrm>
      $<IF:$<TARGET_EXISTS:PkgConfig::DXVA2>,--enable-dxva2,--disable-dxva2>
      $<IF:$<TARGET_EXISTS:PkgConfig::FT2>,--enable-libfreetype,--disable-libfreetype>
      $<IF:$<TARGET_EXISTS:PkgConfig::GNUTLS>,--enable-gnutls,--disable-gnutls>
      $<IF:$<TARGET_EXISTS:PkgConfig::LIBAOM>,--enable-libaom,--disable-libaom>
      $<IF:$<TARGET_EXISTS:PkgConfig::LIBASS>,--enable-libass,--disable-libass>
      $<IF:$<TARGET_EXISTS:PkgConfig::LIBDAV1D>,--enable-libdav1d,--disable-libdav1d>
      $<IF:$<TARGET_EXISTS:PkgConfig::LIBIEC61883>,--enable-libiec61883,--disable-libiec61883>
      $<IF:$<TARGET_EXISTS:PkgConfig::LIBXVID>,--enable-libxvid,--disable-libxvid>
      $<IF:$<TARGET_EXISTS:PkgConfig::VAAPI>,--enable-vaapi,--disable-vaapi>
      $<IF:$<TARGET_EXISTS:PkgConfig::VDPAU>,--enable-vdpau,--disable-vdpau>
      $<IF:$<TARGET_EXISTS:Vulkan::Vulkan>,--enable-vulkan,--disable-vulkan>
    # $<IF:$<TARGET_EXISTS:PkgConfig::SDL2>,--enable-sdl2,--disable-sdl2>
    BUILD_COMMAND ${MAKE_EXECUTABLE} ${MAKE_JFLAG}
    BUILD_ALWAYS ${LIBS_ALWAYS_REBUILD}
    INSTALL_COMMAND ${MAKE_EXECUTABLE} install
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    DEPENDS lame external_libs ${after_libs})

  add_dependencies(embedded_libs FFmpeg)

  message(STATUS "Will build FFmpeg (embedded)")

  ExternalProject_Add_Step(
    FFmpeg install_pkgconfig_files
    DEPENDEES install
    WORKING_DIRECTORY <BINARY_DIR>
    COMMAND ${MAKE_EXECUTABLE} install-pkgconfig)

endfunction()

find_or_build_ffmpeg()
