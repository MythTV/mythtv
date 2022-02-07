#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

#
# Enumerate all headers
#

set(HEADERS_TO_INSTALL
    mythcolourspace.h
    mythdialogbox.h
    mythfontproperties.h
    mythgenerictree.h
    mythgesture.h
    mythhdr.h
    mythimage.h
    mythmainwindow.h
    mythnotification.h
    mythnotificationcenter.h
    mythpainter.h
    mythpainter_qt.h
    mythprogressdialog.h
    mythrect.h
    mythscreenstack.h
    mythscreentype.h
    myththemebase.h
    myththemedmenu.h
    mythuiactions.h
    mythuianimation.h
    mythuibutton.h
    mythuibuttonlist.h
    mythuibuttontree.h
    mythuicheckbox.h
    mythuiclock.h
    mythuicomposite.h
    mythuieditbar.h
    mythuiexp.h
    mythuifilebrowser.h
    mythuigroup.h
    mythuiguidegrid.h
    mythuihelper.h
    mythuiimage.h
    mythuilocation.h
    mythuiprogressbar.h
    mythuiscreenbounds.h
    mythuiscrollbar.h
    mythuishape.h
    mythuisimpletext.h
    mythuispinbox.h
    mythuistatetracker.h
    mythuistatetype.h
    mythuitext.h
    mythuitextedit.h
    mythuithemecache.h
    mythuithemehelper.h
    mythuitype.h
    mythuiutils.h
    mythuivideo.h
    mythuiwebbrowser.h
    mythvirtualkeyboard.h
    themeinfo.h
    x11colors.h
    xmlparsebase.h)

#
# Declare the library
#
add_library(
  mythui
  devices/mythinputdevicehandler.cpp
  devices/mythinputdevicehandler.h
  mythcolourspace.cpp
  mythcolourspace.h
  mythdialogbox.cpp
  mythdialogbox.h
  mythdisplay.cpp
  mythdisplaymode.cpp
  mythedid.cpp
  mythfontmanager.cpp
  mythfontproperties.cpp
  mythfontproperties.h
  mythgenerictree.cpp
  mythgenerictree.h
  mythgesture.cpp
  mythgesture.h
  mythhdr.cpp
  mythhdr.h
  mythimage.cpp
  mythimage.h
  mythmainwindow.cpp
  mythmainwindow.h
  mythmainwindowprivate.cpp
  mythnotification.cpp
  mythnotification.h
  mythnotificationcenter.cpp
  mythnotificationcenter.h
  mythnotificationcenter_private.h
  mythpainter.cpp
  mythpainter.h
  mythpainter_qt.cpp
  mythpainter_qt.h
  mythpaintergpu.cpp
  mythpainterwindow.cpp
  mythpainterwindowqt.cpp
  mythprogressdialog.cpp
  mythprogressdialog.h
  mythrect.cpp
  mythrect.h
  mythrender.cpp
  mythscreensaver.cpp
  mythscreenstack.cpp
  mythscreenstack.h
  mythscreentype.cpp
  mythscreentype.h
  myththemebase.cpp
  myththemebase.h
  myththemedmenu.cpp
  myththemedmenu.h
  mythudplistener.cpp
  mythuiactions.h
  mythuianimation.cpp
  mythuianimation.h
  mythuibutton.cpp
  mythuibutton.h
  mythuibuttonlist.cpp
  mythuibuttonlist.h
  mythuibuttontree.cpp
  mythuibuttontree.h
  mythuicheckbox.cpp
  mythuicheckbox.h
  mythuiclock.cpp
  mythuiclock.h
  mythuicomposite.cpp
  mythuicomposite.h
  mythuieditbar.cpp
  mythuieditbar.h
  mythuiexp.h
  mythuifilebrowser.cpp
  mythuifilebrowser.h
  mythuigroup.cpp
  mythuigroup.h
  mythuiguidegrid.cpp
  mythuiguidegrid.h
  mythuihelper.cpp
  mythuihelper.h
  mythuiimage.cpp
  mythuiimage.h
  mythuilocation.cpp
  mythuilocation.h
  mythuiprocedural.cpp
  mythuiprogressbar.cpp
  mythuiprogressbar.h
  mythuiscreenbounds.cpp
  mythuiscreenbounds.h
  mythuiscrollbar.cpp
  mythuiscrollbar.h
  mythuishape.cpp
  mythuishape.h
  mythuisimpletext.cpp
  mythuisimpletext.h
  mythuispinbox.cpp
  mythuispinbox.h
  mythuistatetracker.cpp
  mythuistatetracker.h
  mythuistatetype.cpp
  mythuistatetype.h
  mythuitext.cpp
  mythuitext.h
  mythuitextedit.cpp
  mythuitextedit.h
  mythuithemecache.cpp
  mythuithemecache.h
  mythuithemehelper.cpp
  mythuithemehelper.h
  mythuitype.cpp
  mythuitype.h
  mythuiutils.cpp
  mythuiutils.h
  mythuivideo.cpp
  mythuivideo.h
  mythvirtualkeyboard.cpp
  mythvirtualkeyboard.h
  mythvrr.cpp
  themeinfo.cpp
  themeinfo.h
  x11colors.cpp
  x11colors.h
  xmlparsebase.cpp
  xmlparsebase.h)

#
# All remaining target information
#

target_compile_definitions(mythui PRIVATE MUI_API)

target_include_directories(
  mythui
  PRIVATE .
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
  INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythui
  PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network
         Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Widgets
         Qt${QT_VERSION_MAJOR}::Xml mythbase)

#
# Conditionally included code.
#

# AndroidExtras only exists for Qt5
if(TARGET Qt5::AndroidExtras)
  target_link_libraries(mythui PUBLIC Qt5::AndroidExtras)
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::WebKit)
  target_sources(mythui PRIVATE mythuiwebbrowser.cpp mythuiwebbrowser.h)
  target_link_libraries(mythui PUBLIC Qt${QT_VERSION_MAJOR}::WebKit
                                      Qt${QT_VERSION_MAJOR}::WebKitWidgets)
endif()

if(TARGET X11::X11)
  target_link_libraries(mythui PRIVATE X11::X11 X11::Xrandr X11::Xrender)
  if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    target_link_libraries(mythui PRIVATE X11::Xext X11::Xxf86vm)
  endif()
  target_sources(
    mythui
    PRIVATE platforms/mythxdisplay.cpp
            platforms/mythdisplayx11.cpp
            platforms/mythdisplayx11.h
            platforms/mythxdisplay.h
            platforms/mythscreensaverx11.cpp
            platforms/mythscreensaverx11.h
            platforms/mythnvcontrol.cpp
            platforms/mythnvcontrol.h)
endif()

if(TARGET PkgConfig::LibCEC)
  target_link_libraries(mythui PRIVATE PkgConfig::LibCEC)
  target_sources(mythui PRIVATE devices/mythcecadapter.cpp
                                devices/mythcecadapter.h)
endif()

if(TARGET PkgConfig::DRM)
  target_link_libraries(mythui PRIVATE PkgConfig::DRM)
  target_sources(
    mythui
    PRIVATE platforms/mythscreensaverdrm.cpp
            platforms/mythscreensaverdrm.h
            platforms/mythdisplaydrm.cpp
            platforms/mythdisplaydrm.h
            platforms/mythdrmdevice.cpp
            platforms/mythdrmdevice.h
            platforms/drm/mythdrmconnector.cpp
            platforms/drm/mythdrmconnector.h
            platforms/drm/mythdrmcrtc.cpp
            platforms/drm/mythdrmcrtc.h
            platforms/drm/mythdrmencoder.cpp
            platforms/drm/mythdrmencoder.h
            platforms/drm/mythdrmframebuffer.cpp
            platforms/drm/mythdrmframebuffer.h
            platforms/drm/mythdrmmode.cpp
            platforms/drm/mythdrmmode.h
            platforms/drm/mythdrmplane.cpp
            platforms/drm/mythdrmplane.h
            platforms/drm/mythdrmproperty.cpp
            platforms/drm/mythdrmproperty.h
            platforms/drm/mythdrmresources.cpp
            platforms/drm/mythdrmresources.h
            platforms/drm/mythdrmvrr.cpp
            platforms/drm/mythdrmvrr.h)

  if(TARGET Qt5::GuiPrivate) # Not completed for Qt6 yet.
    target_sources(mythui PRIVATE platforms/drm/mythdrmhdr.cpp
                                  platforms/drm/mythdrmhdr.h)
  endif()
endif()

if(TARGET Qt5::GuiPrivate) # Not completed for Qt6 yet.
  target_link_libraries(mythui PRIVATE Qt${QT_VERSION_MAJOR}::GuiPrivate)
  if(TARGET PkgConfig::WAYLAND_CLIENT)
    target_link_libraries(mythui PRIVATE PkgConfig::WAYLAND_CLIENT)
    target_sources(
      mythui
      PRIVATE platforms/mythscreensaverwayland.cpp
              platforms/mythscreensaverwayland.h
              platforms/mythwaylandextras.cpp
              platforms/mythwaylandextras.h
              platforms/waylandprotocols/idle_inhibit_unstable_v1.h)
    # idle_inhibit_unstable_v1.c is not listed here as a source because it is
    # directly included by mythscreensaverwayland.cpp. If listed, cmake tries to
    # compile it separately, which fails.
  endif()
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::DBus)
  target_link_libraries(mythui PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
  target_sources(
    mythui
    PRIVATE platforms/mythdisplaymutter.cpp platforms/mythdisplaymutter.h
            platforms/mythscreensaverdbus.cpp platforms/mythscreensaverdbus.h)
endif()

if(_HAVE_GL_OR_GLES)
  if(TARGET OpenGL::GL)
    target_link_libraries(mythui PRIVATE Qt${QT_VERSION_MAJOR}::OpenGL
                                         OpenGL::GL)
    if(TARGET OpenGL::EGL)
      target_link_libraries(mythui PRIVATE OpenGL::EGL)
    endif()
  else()
    target_link_libraries(mythui PUBLIC Qt${QT_VERSION_MAJOR}::OpenGL
                                        PkgConfig::GLES2)
    if(TARGET PkgConfig::EGL)
      target_link_libraries(mythui PRIVATE PkgConfig::EGL)
    endif()
  endif()
  target_sources(
    mythui
    PRIVATE opengl/mythegl.cpp
            opengl/mythegl.h
            opengl/mythopenglperf.cpp
            opengl/mythopenglperf.h
            opengl/mythpainteropengl.cpp
            opengl/mythpainteropengl.h
            opengl/mythpainterwindowopengl.cpp
            opengl/mythpainterwindowopengl.h
            opengl/mythrenderopengl.cpp
            opengl/mythrenderopengl.h)
endif(_HAVE_GL_OR_GLES)

if(TARGET Vulkan::Vulkan)
  target_link_libraries(mythui PUBLIC Vulkan::Vulkan)
  target_sources(
    mythui
    PRIVATE vulkan/mythcombobuffervulkan.cpp
            vulkan/mythcombobuffervulkan.h
            vulkan/mythdebugvulkan.cpp
            vulkan/mythdebugvulkan.h
            vulkan/mythpaintervulkan.cpp
            vulkan/mythpaintervulkan.h
            vulkan/mythpainterwindowvulkan.cpp
            vulkan/mythpainterwindowvulkan.h
            vulkan/mythrendervulkan.cpp
            vulkan/mythrendervulkan.h
            vulkan/mythshadervulkan.cpp
            vulkan/mythshadervulkan.h
            vulkan/mythtexturevulkan.cpp
            vulkan/mythtexturevulkan.h
            vulkan/mythuniformbuffervulkan.cpp
            vulkan/mythuniformbuffervulkan.h
            vulkan/mythvertexbuffervulkan.cpp
            vulkan/mythvertexbuffervulkan.h
            vulkan/mythwindowvulkan.cpp
            vulkan/mythwindowvulkan.h)
endif()
if(ANDROID)
  target_sources(
    mythui
    PRIVATE platforms/mythdisplayandroid.cpp platforms/mythdisplayandroid.h
            platforms/mythscreensaverandroid.cpp
            platforms/mythscreensaverandroid.h)
endif()

if(WIN32)
  target_sources(mythui PRIVATE mythpainter_d3d9.cpp mythrender_d3d9.cpp
                                platforms/mythdisplaywindows.cpp)
  target_compile_definitions(mythui PRIVATE NODRAWTEXT)
  target_link_libraries(mythui PUBLIC user32 gdi32)
  if(HAVE_DXVA2)
    target_compile_definitions(mythui PRIVATE USING_DXVA2)
  endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  target_link_libraries(mythui PRIVATE ${APPLE_COCOA_LIBRARY}
                                       ${APPLE_IOKIT_LIBRARY})
  target_sources(
    mythui
    PRIVATE platforms/mythscreensaverosx.cpp
            platforms/mythscreensaverosx.h
            platforms/mythdisplayosx.cpp
            platforms/mythdisplayosx.h
            platforms/mythosxutils.cpp
            platforms/mythosxutils.h
            platforms/mythutilscocoa.h
            platforms/mythutilscocoa.mm
            # Darwin implies APPLEREMOTE
            devices/AppleRemote.cpp
            devices/AppleRemote.h
            devices/AppleRemoteListener.cpp
            devices/AppleRemoteListener.h)
  target_compile_definitions(mythui PRIVATE USING_APPLEREMOTE)
  if(NOT ENABLE_LIRC)
    target_sources(mythui PRIVATE devices/lircevent.cpp devices/lircevent.h)
  endif()
endif()

if(TARGET joystick)
  target_sources(mythui PRIVATE devices/jsmenu.cpp devices/jsmenu.h
                                devices/jsmenuevent.cpp devices/jsmenuevent.h)
  target_compile_definitions(mythui PRIVATE USE_JOYSTICK_MENU)
  target_link_libraries(mythui PUBLIC joystick)
endif()

if(ENABLE_LIRC AND NOT WIN32)
  target_sources(
    mythui
    PRIVATE devices/lirc.cpp devices/lirc.h devices/lirc_client.cpp
            devices/lirc_client.h devices/lircevent.cpp devices/lircevent.h)
  target_compile_definitions(mythui PRIVATE USE_LIRC)
endif()

if(TARGET mythtv_mmal)
  target_link_libraries(mythui PRIVATE mythtv_mmal)
  target_sources(mythui PRIVATE platforms/mythdisplayrpi.cpp
                                platforms/mythdisplayrpi.h)
endif()

#
# Installation section
#

install(TARGETS mythui LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${HEADERS_TO_INSTALL}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythui)
