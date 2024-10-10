#
# Copyright (C) 2022-2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# These are the libraries provided by Android ndk 25 for arch64-linux-android29
#
# ~~~
# libaaudio.so
# libamidi.so
# libandroid.so
# libbinder_ndk.so
# libc++.so
# libc.so
# libcamera2ndk.so
# libdl.so
# libEGL.so
# libGLESv1_CM.so
# libGLESv2.so
# libGLESv3.so
# libjnigraphics.so
# liblog.so
# libm.so
# libmediandk.so
# libnativewindow.so
# libneuralnetworks.so
# libOpenMAXAL.so
# libOpenSLES.so
# libstdc++.so
# libsync.so
# libvulkan.so
# libz.so
# ~~~

include(GetDefine)

# ~~~
# In file .../sysroot/usr/include/iconv.h
#
# #if __ANDROID_API__ >= 28
# iconv_t iconv_open(const char* __src_encoding, const char* __dst_encoding) __INTRODUCED_IN(28);
# ...
# #endif /* __ANDROID_API__ >= 28 */
# Functions can be found in libc.so using the names iconv/iconv_open/iconv_close
# ~~~

#
# Gather version numbers needed to create the pkgconfig files
#
get_define(vulkan/vulkan_core.h VK_HEADER_VERSION
           "^#define +VK_HEADER_VERSION +([0-9]+)$")

# There are no header files that provides exact version numbers for OpenGL.
# These version numbers are based on the presence of #ifdef statements in the
# header files.
set(EGL_VERSION 1.5)
set(GLES1_VERSION 1.0)
set(GLES2_VERSION 2.0)
set(GLES3_VERSION 3.2)

#
# Create all the pkgconfig files
#
foreach(NAME IN ITEMS egl glesv1_cm glesv2 glesv3 vulkan)
  if(NOT EXISTS ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/${NAME}.pc)
    message(
      STATUS "Created file ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/${NAME}.pc")
    configure_file(${PROJECT_SOURCE_DIR}/cmake/files/android/${NAME}.pc.in
                   ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/${NAME}.pc @ONLY)
  endif()
endforeach(NAME)

if(ANDROID_MIN_SDK_VERSION VERSION_GREATER_EQUAL 31)
  if(NOT EXISTS ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/icu-i18n.pc)
    message(
      STATUS "Created file ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/icu-i18n.pc")
    configure_file(${PROJECT_SOURCE_DIR}/cmake/files/android/icu-i18n.pc.in
                   ${LIBS_INSTALL_PREFIX}/lib/pkgconfig/icu-i18n.pc @ONLY)
  endif()
endif()
