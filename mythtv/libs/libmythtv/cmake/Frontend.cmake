#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT ENABLE_FRONTEND)
  return()
endif()

target_sources(
  mythtv
  PRIVATE # DVD
          DVD/mythdvdplayer.h
          DVD/mythdvddecoder.h
          DVD/mythdvdplayer.cpp
          DVD/mythdvddecoder.cpp
          # Bluray
          Bluray/mythbdplayer.h
          Bluray/mythbddecoder.h
          Bluray/mythbdoverlayscreen.h
          Bluray/mythbdplayer.cpp
          Bluray/mythbddecoder.cpp
          Bluray/mythbdoverlayscreen.cpp
          # Recording profile stuff
          profilegroup.h
          profilegroup.cpp
          # Video playback
          tv_play.h
          tvplaybackstate.h
          mythtvmenu.h
          mythtvactionutils.h
          mythplayer.h
          mythplayerstate.h
          mythplayeruibase.h
          mythplayerui.h
          mythplayereditorui.h
          mythplayervideoui.h
          mythplayercaptionsui.h
          mythplayeroverlayui.h
          mythvideoscantracker.h
          mythplayervisualiserui.h
          mythplayeravsync.h
          mythplayeraudioui.h
          audioplayer.h
          mythccextractorplayer.h
          captions/teletextextractorreader.h
          playercontext.h
          tv_play_win.h
          deletemap.h
          mythcommflagplayer.h
          commbreakmap.h
          mythpreviewplayer.h
          tvbrowsehelper.h
          mheg/netstream.h
          audioplayer.cpp
          captions/teletextextractorreader.cpp
          commbreakmap.cpp
          deletemap.cpp
          mheg/netstream.cpp
          mythccextractorplayer.cpp
          mythcommflagplayer.cpp
          mythplayer.cpp
          mythplayeraudioui.cpp
          mythplayeravsync.cpp
          mythplayercaptionsui.cpp
          mythplayereditorui.cpp
          mythplayeroverlayui.cpp
          mythplayerstate.cpp
          mythplayerui.cpp
          mythplayeruibase.cpp
          mythplayervideoui.cpp
          mythplayervisualiserui.cpp
          mythpreviewplayer.cpp
          mythtvmenu.cpp
          mythvideoscantracker.cpp
          playercontext.cpp
          tv_play.cpp
          tv_play_win.cpp
          tvbrowsehelper.cpp
          tvplaybackstate.cpp
          # Input/output
          io/mythiowrapper.h
          io/mythiowrapper.cpp
          # Text subtitle parser
          captions/textsubtitleparser.h
          captions/textsubtitleparser.cpp
          # A/V decoders
          decoders/decoderbase.h
          decoders/avformatdecoder.h
          decoders/mythcodeccontext.h
          decoders/mythdecoderthread.h
          decoders/avformatdecoder.cpp
          decoders/decoderbase.cpp
          decoders/mythcodeccontext.cpp
          decoders/mythdecoderthread.cpp
          # On screen display (video output overlay)
          osd.h
          mythmediaoverlay.h
          mythcaptionsoverlay.h
          captions/teletextscreen.h
          captions/subtitlescreen.h
          overlays/mythnavigationoverlay.h
          overlays/mythchanneloverlay.h
          mheg/interactivescreen.h
          osd.cpp
          mythmediaoverlay.cpp
          mythcaptionsoverlay.cpp
          captions/teletextscreen.cpp
          captions/subtitlescreen.cpp
          overlays/mythnavigationoverlay.cpp
          overlays/mythchanneloverlay.cpp
          mheg/interactivescreen.cpp
          # Video output
          mythvideoout.h
          mythvideooutnull.h
          mythvideooutgpu.h
          mythvideogpu.h
          videobuffers.h
          jitterometer.h
          mythvideoprofile.h
          mythcodecid.h
          videoouttypes.h
          mythvideobounds.h
          mythvideocolourspace.h
          visualisations/videovisual.h
          visualisations/videovisualdefs.h
          visualisations/videovisualspectrum.h
          mythdeinterlacer.h
          mythinteropgpu.h
          mythvideoout.cpp
          mythvideooutnull.cpp
          mythvideooutgpu.cpp
          mythvideogpu.cpp
          videobuffers.cpp
          jitterometer.cpp
          mythvideoprofile.cpp
          mythcodecid.cpp
          mythvideobounds.cpp
          mythvideocolourspace.cpp
          visualisations/videovisual.cpp
          visualisations/videovisualspectrum.cpp
          mythdeinterlacer.cpp
          mythinteropgpu.cpp
          decoders/mythdrmprimecontext.h
          decoders/mythdrmprimecontext.cpp
          # Misc. frontend
          DetectLetterbox.h
          DetectLetterbox.cpp)

if(TARGET PkgConfig::LIBASS)
  target_link_libraries(mythtv PUBLIC PkgConfig::LIBASS)
endif()

# Note - all OpenGL/EGL interop files are added under using_opengl... They are
# however referenced, without ifdef guards, from the relevant following files.
# It is essentially assumed that any given hardware acceleration must have
# associated OpenGL support (e.g. there is no VDPAU without a functioning NVidia
# driver)
if(TARGET mythtv_mmal)
  target_link_libraries(mythtv PRIVATE mythtv_mmal)
  target_sources(mythtv PRIVATE decoders/mythmmalcontext.cpp
                                decoders/mythmmalcontext.h)
endif()

if(TARGET PkgConfig::VDPAU AND TARGET X11::X11)
  target_link_libraries(mythtv PUBLIC PkgConfig::VDPAU)
  target_sources(
    mythtv PRIVATE decoders/mythvdpaucontext.cpp decoders/mythvdpaucontext.h
                   decoders/mythvdpauhelper.cpp decoders/mythvdpauhelper.h)
endif()

if(TARGET PkgConfig::DRM)
  target_link_libraries(mythtv PUBLIC PkgConfig::DRM)
  if(TARGET Qt${QT_VERSION_MAJOR}::GuiPrivate)
    target_link_libraries(mythtv PUBLIC Qt${QT_VERSION_MAJOR}::GuiPrivate)
    target_sources(
      mythtv
      PRIVATE drm/mythvideodrm.cpp drm/mythvideodrmbuffer.cpp
              drm/mythvideodrmutils.cpp drm/mythvideodrm.h
              drm/mythvideodrmbuffer.h drm/mythvideodrmutils.h)
  endif()
endif()

if(TARGET PkgConfig::VAAPI)
  target_link_libraries(mythtv PUBLIC PkgConfig::VAAPI)
  target_sources(mythtv PRIVATE decoders/mythvaapicontext.cpp
                                decoders/mythvaapicontext.h)
endif()

if(TARGET PkgConfig::FFNVCODEC)
  target_link_libraries(mythtv PUBLIC PkgConfig::FFNVCODEC)
  target_sources(mythtv PRIVATE decoders/mythnvdeccontext.h
                                decoders/mythnvdeccontext.cpp)
endif()

if(TARGET mediacodec)
  target_link_libraries(mythtv PUBLIC mediacodec)
  target_sources(mythtv PRIVATE decoders/mythmediacodeccontext.cpp
                                decoders/mythmediacodeccontext.h)
endif()

if(TARGET Vulkan::Vulkan)
  target_link_libraries(mythtv PUBLIC Vulkan::Vulkan)
  target_sources(
    mythtv
    PRIVATE vulkan/mythvideovulkan.h
            vulkan/mythvideooutputvulkan.h
            vulkan/mythvideotexturevulkan.h
            vulkan/mythvideoshadersvulkan.h
            vulkan/mythvideovulkan.cpp
            vulkan/mythvideooutputvulkan.cpp
            vulkan/mythvideotexturevulkan.cpp
            visualisations/vulkan/mythvisualvulkan.cpp
            visualisations/vulkan/mythvisualcirclesvulkan.cpp
            visualisations/vulkan/mythvisualmonoscopevulkan.cpp
            visualisations/vulkan/mythvisualvulkan.h
            visualisations/vulkan/mythvisualcirclesvulkan.h
            visualisations/vulkan/mythvisualmonoscopevulkan.h)
endif()

if(TARGET Vulkan::Vulkan OR TARGET any_opengl)
  target_sources(mythtv PRIVATE visualisations/videovisualmonoscope.cpp
                                visualisations/videovisualmonoscope.h)
endif()

if(TARGET any_opengl)
  target_link_libraries(mythtv PUBLIC any_opengl)
  target_sources(
    mythtv
    PRIVATE opengl/mythopenglvideo.h
            opengl/mythvideooutopengl.h
            opengl/mythopenglvideoshaders.h
            opengl/mythopenglinterop.h
            opengl/mythvideotextureopengl.h
            opengl/mythopengltonemap.h
            opengl/mythopenglcomputeshaders.h
            visualisations/videovisualcircles.h
            opengl/mythopenglvideo.cpp
            opengl/mythvideooutopengl.cpp
            opengl/mythopenglinterop.cpp
            opengl/mythvideotextureopengl.cpp
            opengl/mythopengltonemap.cpp
            visualisations/videovisualcircles.cpp
            visualisations/opengl/mythvisualmonoscopeopengl.cpp
            visualisations/opengl/mythvisualmonoscopeopengl.h)
  if(TARGET PkgConfig::VAAPI)
    target_sources(
      mythtv PRIVATE opengl/mythvaapiinterop.h opengl/mythvaapiglxinterop.h
                     opengl/mythvaapiinterop.cpp opengl/mythvaapiglxinterop.cpp)
    if(TARGET PkgConfig::VDPAU AND TARGET X11::X11)
      target_sources(mythtv PRIVATE opengl/mythvdpauinterop.h
                                    opengl/mythvdpauinterop.cpp)
    endif()
  endif()

  if(TARGET mediacodec)
    target_sources(mythtv PRIVATE opengl/mythmediacodecinterop.cpp
                                  opengl/mythmediacodecinterop.h)
  endif()
  if(TARGET PkgConfig::FFNVCODEC)
    target_sources(mythtv PRIVATE opengl/mythnvdecinterop.cpp
                                  opengl/mythnvdecinterop.h)
  endif()
  if(APPLE_VIDEOTOOLBOX_LIBRARY)
    target_sources(mythtv PRIVATE opengl/mythvtbinterop.cpp
                                  opengl/mythvtbinterop.h)
  endif()
  if(TARGET OpenGL::EGL OR TARGET PkgConfig::EGL)
    target_sources(
      mythtv
      PRIVATE opengl/mythegldefs.h opengl/mythegldmabuf.h
              opengl/mythdrmprimeinterop.h opengl/mythdrmprimeinterop.cpp
              opengl/mythegldmabuf.cpp)
    if(TARGET mythtv_mmal)
      target_sources(mythtv PRIVATE opengl/mythmmalinterop.cpp
                                    opengl/mythmmalinterop.h)
    endif()
    if(TARGET PkgConfig::VAAPI)
      target_sources(mythtv PRIVATE opengl/mythvaapidrminterop.cpp
                                    opengl/mythvaapidrminterop.h)
    endif()
  endif()

  target_sources(
    mythtv
    PRIVATE visualisations/goom/filters.h
            visualisations/goom/goomconfig.h
            visualisations/goom/goom_core.h
            visualisations/goom/goom_tools.h
            visualisations/goom/graphic.h
            visualisations/goom/ifs.h
            visualisations/goom/lines.h
            visualisations/goom/drawmethods.h
            visualisations/goom/mmx.h
            visualisations/goom/mathtools.h
            visualisations/goom/surf3d.h
            visualisations/goom/tentacle3d.h
            visualisations/goom/v3d.h
            visualisations/goom/zoom_filters.h
            visualisations/videovisualgoom.h
            visualisations/goom/filters.cpp
            visualisations/goom/goom_core.cpp
            visualisations/goom/graphic.cpp
            visualisations/goom/tentacle3d.cpp
            visualisations/goom/ifs.cpp
            visualisations/goom/ifs_display.cpp
            visualisations/goom/lines.cpp
            visualisations/goom/surf3d.cpp
            visualisations/goom/zoom_filter_mmx.cpp
            visualisations/goom/zoom_filter_xmmx.cpp
            visualisations/videovisualgoom.cpp)
endif(TARGET any_opengl)

if(TARGET PkgConfig::LIBDNS_SD)
  target_link_libraries(mythtv PUBLIC PkgConfig::LIBDNS_SD)
  if(TARGET PkgConfig::LIBCRYPTO)
    target_sources(
      mythtv
      PRIVATE AirPlay/mythairplayserver.h AirPlay/mythraopdevice.h
              AirPlay/mythraopconnection.h AirPlay/mythairplayserver.cpp
              AirPlay/mythraopdevice.cpp AirPlay/mythraopconnection.cpp)
  endif()
endif()

if(TARGET mythtv_mheg)
  target_link_libraries(mythtv PUBLIC mythtv_mheg mythfreemheg PkgConfig::FT2)
  target_sources(
    mythtv
    PRIVATE # DSMCC stuff
            mheg/dsmcc.h
            mheg/dsmcccache.h
            mheg/dsmccbiop.h
            mheg/dsmccobjcarousel.h
            mheg/dsmccreceiver.h
            mheg/dsmcc.cpp
            mheg/dsmccbiop.cpp
            mheg/dsmcccache.cpp
            mheg/dsmccobjcarousel.cpp
            # MHEG interaction channel
            mheg/mhegic.h
            mheg/mhegic.cpp
            # MHEG/MHI stuff
            mheg/interactivetv.h
            mheg/mhi.h
            mheg/interactivetv.cpp
            mheg/mhi.cpp)
endif()

if(TARGET PkgConfig::V4L2)
  target_link_libraries(mythtv PUBLIC PkgConfig::V4L2)
  target_sources(mythtv PRIVATE decoders/mythv4l2m2mcontext.cpp
                                decoders/mythv4l2m2mcontext.h)
endif()

if(WIN32 AND ENABLE_D3D)
  target_sources(mythtv PRIVATE videoout_d3d.cpp videoout_d3d.h)
  if(HAVE_DXVA2)
    target_sources(mythtv PRIVATE dxva2decoder.cpp dxva2decoder.h)
  endif()
endif()
