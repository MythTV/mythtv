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
    langsettings.h
    mediamonitor.h
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
    mythterminal.h
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
    mythvirtualkeyboard.h
    schemawizard.h
    standardsettings.h
    storagegroupeditor.h
    themeinfo.h
    x11colors.h
    xmlparsebase.h)

set(LIBMYTHUI_HEADERS_NOT_INSTALLED
    devices/mythinputdevicehandler.h
    dbsettings.h
    guistartup.h
    mythdisplay.h
    mythdisplaymode.h
    mythedid.h
    mythfontmanager.h
    mythmainwindowprivate.h
    mythnotificationcenter_private.h
    mythpaintergpu.h
    mythpainterwindow.h
    mythpainterwindowqt.h
    mythrender_base.h
    mythscreensaver.h
    mythudplistener.h
    mythuiprocedural.h
    mythvrr.h
    rawsettingseditor.h
    )
#
# Declare the library
#
add_library(
  mythui
  ${HEADERS_TO_INSTALL}
  ${LIBMYTHUI_HEADERS_NOT_INSTALLED}
  devices/mythinputdevicehandler.cpp
  dbsettings.cpp
  guistartup.cpp
  langsettings.cpp
  mediamonitor.cpp
  mythcolourspace.cpp
  mythdialogbox.cpp
  mythdisplay.cpp
  mythdisplaymode.cpp
  mythedid.cpp
  mythfontmanager.cpp
  mythfontproperties.cpp
  mythgenerictree.cpp
  mythgesture.cpp
  mythhdr.cpp
  mythimage.cpp
  mythmainwindow.cpp
  mythmainwindowprivate.cpp
  mythnotification.cpp
  mythnotificationcenter.cpp
  mythpainter.cpp
  mythpainter_qt.cpp
  mythpaintergpu.cpp
  mythpainterwindow.cpp
  mythpainterwindowqt.cpp
  mythprogressdialog.cpp
  mythrect.cpp
  mythrender.cpp
  mythscreensaver.cpp
  mythscreenstack.cpp
  mythscreentype.cpp
  mythterminal.cpp
  myththemebase.cpp
  myththemedmenu.cpp
  mythudplistener.cpp
  mythuianimation.cpp
  mythuibutton.cpp
  mythuibuttonlist.cpp
  mythuibuttontree.cpp
  mythuicheckbox.cpp
  mythuiclock.cpp
  mythuicomposite.cpp
  mythuieditbar.cpp
  mythuifilebrowser.cpp
  mythuigroup.cpp
  mythuiguidegrid.cpp
  mythuihelper.cpp
  mythuiimage.cpp
  mythuilocation.cpp
  mythuiprocedural.cpp
  mythuiprogressbar.cpp
  mythuiscreenbounds.cpp
  mythuiscrollbar.cpp
  mythuishape.cpp
  mythuisimpletext.cpp
  mythuispinbox.cpp
  mythuistatetracker.cpp
  mythuistatetype.cpp
  mythuitext.cpp
  mythuitextedit.cpp
  mythuithemecache.cpp
  mythuithemehelper.cpp
  mythuitype.cpp
  mythuiutils.cpp
  mythuivideo.cpp
  mythvirtualkeyboard.cpp
  mythvrr.cpp
  rawsettingseditor.cpp
  schemawizard.cpp
  standardsettings.cpp
  storagegroupeditor.cpp
  themeinfo.cpp
  x11colors.cpp
  xmlparsebase.cpp
  )

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

if(TARGET Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
  list(APPEND HEADERS_TO_INSTALL mythuiwebbrowser.h)
  target_sources(mythui PRIVATE mythuiwebbrowser.cpp mythuiwebbrowser.h)
  target_link_libraries(mythui PUBLIC Qt${QT_VERSION_MAJOR}::Quick
                                      Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
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

  if(TARGET Qt${QT_VERSION_MAJOR}::GuiPrivate)
    target_sources(mythui PRIVATE platforms/drm/mythdrmhdr.cpp
                                  platforms/drm/mythdrmhdr.h)
  endif()
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::GuiPrivate)
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

if(TARGET any_opengl)
  target_link_libraries(mythui PUBLIC any_opengl)
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
endif(TARGET any_opengl)

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

if(UNIX)
  target_sources(mythui PRIVATE mediamonitor-unix.cpp mediamonitor-unix.h)
  if(NOT TARGET ${QT_PKG_NAME}::DBus)
    set_source_files_properties(mediamonitor-unix.h PROPERTIES SKIP_AUTOMOC
                                                               TRUE)
  endif()
endif()

if(WIN32)
  target_sources(mythui PRIVATE mythpainter_d3d9.cpp mythrender_d3d9.cpp
                                mediamonitor-windows.cpp mediamonitor-windows.h
                                platforms/mythdisplaywindows.cpp)
  target_compile_definitions(mythui PRIVATE NODRAWTEXT)
  target_link_libraries(mythui PUBLIC user32 gdi32)
endif()

if(MINGW OR win32-msvc)
  target_sources(mythui PRIVATE mediamonitor-windows.cpp mediamonitor-windows.h)
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
  if(NOT TARGET lirc)
    target_sources(mythui PRIVATE devices/lircevent.cpp devices/lircevent.h)
  endif()
  if(APPLE_DISKARBITRATION_LIBRARY)
    target_sources(mythui PRIVATE mediamonitor-darwin.cpp mediamonitor-darwin.h)
    target_link_libraries(mythui PRIVATE ${APPLE_DISKARBITRATION_LIBRARY})
  endif()
endif()

if(TARGET joystick)
  target_sources(mythui PRIVATE devices/jsmenu.cpp devices/jsmenu.h
                                devices/jsmenuevent.cpp devices/jsmenuevent.h)
  target_link_libraries(mythui PUBLIC joystick)
endif()

if(TARGET lirc)
  target_link_libraries(mythui PRIVATE lirc)
  target_sources(
    mythui
    PRIVATE devices/lirc.cpp devices/lirc.h devices/lirc_client.cpp
            devices/lirc_client.h devices/lircevent.cpp devices/lircevent.h)
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
