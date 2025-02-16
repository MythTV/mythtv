include ( ../../settings.pro )

QT += network xml sql widgets
android: QT += androidextras

TEMPLATE = lib
TARGET = mythtv-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

POSTINC =

# Needed for FFmpeg 4.3
DEFINES += __STDC_CONSTANT_MACROS

contains(INCLUDEPATH, /usr/include) {
  POSTINC += /usr/include
  INCLUDEPATH -= /usr/include
}
contains(INCLUDEPATH, /usr/local/include) {
  POSTINC += /usr/local/include
  INCLUDEPATH -= /usr/local/include
}
contains(INCLUDEPATH, /usr/X11R6/include) {
  POSTINC += /usr/X11R6/include
  INCLUDEPATH -= /usr/X11R6/include
}


INCLUDEPATH += .. ../.. # for avlib headers
INCLUDEPATH += ../.. ../../external/FFmpeg

!win32-msvc* {
    QMAKE_CXXFLAGS += $${FREETYPE_CFLAGS}
}

mingw: LIBS += -liconv

macx {
    OBJECTIVE_HEADERS += util-osx.h
    OBJECTIVE_SOURCES += util-osx.mm

    # Mac OS X Frameworks
    LIBS += -framework ApplicationServices
    LIBS += -framework Cocoa
    LIBS += -framework CoreServices
    LIBS += -framework CoreFoundation
    LIBS += -framework OpenGL
    LIBS += -framework IOKit

    using_videotoolbox {
        LIBS += -framework CoreVideo
        LIBS += -framework VideoToolbox
        LIBS += -framework IOSurface
        HEADERS += decoders/mythvtbcontext.h
        SOURCES += decoders/mythvtbcontext.cpp
    }

    using_firewire:using_backend {
        QMAKE_CXXFLAGS += -F$${CONFIG_MAC_AVC}
    }

    LIBS += -liconv

    # Qt6 moved QtGui to use metal, link in QtOpenGL until migrated fully to
    # Metal per https://doc.qt.io/qt-6/opengl-changes-qt6.html
    equals(QT_MAJOR_VERSION, 6) {
        QT += opengl
    }
}

cygwin:QMAKE_LFLAGS_SHLIB += -Wl,--noinhibit-exec
cygwin:DEFINES += _WIN32

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

##########################################################################
# libmythtv proper

# Headers needed by frontend & backend
HEADERS += mythframe.h

# Misc. needed by backend/frontend
HEADERS += mythtvexp.h
HEADERS += bitreader.h
HEADERS += bytereader.h
HEADERS += recordinginfo.h
HEADERS += dbcheck.h
HEADERS += videodbcheck.h
HEADERS += tvremoteutil.h           tv.h
HEADERS += jobqueue.h
HEADERS += recordingprofile.h
HEADERS += remoteencoder.h          videosource.h
HEADERS += cardutil.h               sourceutil.h
HEADERS += videometadatautil.h
HEADERS += captions/vbi608extractor.h
HEADERS += captions/cc608decoder.h
HEADERS += captions/cc608reader.h
HEADERS += captions/cc708decoder.h
HEADERS += captions/cc708reader.h
HEADERS += captions/cc708window.h
HEADERS += captions/subtitlereader.h
HEADERS += scheduledrecording.h
HEADERS += signalmonitorvalue.h     signalmonitorlistener.h
HEADERS += livetvchain.h            playgroup.h
HEADERS += channelsettings.h
HEADERS += previewgenerator.h       previewgeneratorqueue.h
HEADERS += transporteditor.h        listingsources.h
HEADERS += restoredata.h
HEADERS += channelgroup.h
HEADERS += recordingrule.h
HEADERS += mythsystemevent.h
HEADERS += io/mythmediabuffer.h
HEADERS += io/mythavformatbuffer.h
HEADERS += io/mythfilebuffer.h
HEADERS += io/mythstreamingbuffer.h
HEADERS += io/mythinteractivebuffer.h
HEADERS += io/mythopticalbuffer.h
HEADERS += metadataimagehelper.h
HEADERS += mythavbufferref.h
HEADERS += mythavrational.h
HEADERS += mythavutil.h
HEADERS += recordingfile.h
HEADERS += driveroption.h
HEADERS += mythhdrvideometadata.h
HEADERS += mythhdrtracker.h
HEADERS += scantype.h

SOURCES += bytereader.cpp
SOURCES += recordinginfo.cpp
SOURCES += dbcheck.cpp
SOURCES += videodbcheck.cpp
SOURCES += tvremoteutil.cpp         tv.cpp
SOURCES += jobqueue.cpp
SOURCES += recordingprofile.cpp
SOURCES += remoteencoder.cpp        videosource.cpp
SOURCES += cardutil.cpp             sourceutil.cpp
SOURCES += videometadatautil.cpp
SOURCES += captions/vbi608extractor.cpp
SOURCES += captions/cc608decoder.cpp
SOURCES += captions/cc608reader.cpp
SOURCES += captions/cc708decoder.cpp
SOURCES += captions/cc708reader.cpp
SOURCES += captions/cc708window.cpp
SOURCES += captions/subtitlereader.cpp
SOURCES += scheduledrecording.cpp
SOURCES += signalmonitorvalue.cpp
SOURCES += livetvchain.cpp          playgroup.cpp
SOURCES += channelsettings.cpp
SOURCES += previewgenerator.cpp     previewgeneratorqueue.cpp
SOURCES += transporteditor.cpp
SOURCES += restoredata.cpp
SOURCES += channelgroup.cpp
SOURCES += recordingrule.cpp
SOURCES += mythsystemevent.cpp
SOURCES += io/mythmediabuffer.cpp
SOURCES += io/mythavformatbuffer.cpp
SOURCES += io/mythfilebuffer.cpp
SOURCES += io/mythstreamingbuffer.cpp
SOURCES += io/mythinteractivebuffer.cpp
SOURCES += io/mythopticalbuffer.cpp
SOURCES += metadataimagehelper.cpp
SOURCES += mythframe.cpp
SOURCES += mythavbufferref.cpp
SOURCES += mythavutil.cpp
SOURCES += recordingfile.cpp
SOURCES += mythhdrvideometadata.cpp
SOURCES += mythhdrtracker.cpp

# DiSEqC
HEADERS += diseqc.h                 diseqcsettings.h
SOURCES += diseqc.cpp               diseqcsettings.cpp

# File/FIFO Writer classes
HEADERS += io/mythmediawriter.h
HEADERS += io/mythavformatwriter.h
HEADERS += io/mythfifowriter.h
SOURCES += io/mythmediawriter.cpp
SOURCES += io/mythavformatwriter.cpp
SOURCES += io/mythfifowriter.cpp

# Teletext stuff
HEADERS += captions/teletextdecoder.h
HEADERS += captions/teletextreader.h
HEADERS += captions/vbilut.h
SOURCES += captions/teletextdecoder.cpp
SOURCES += captions/teletextreader.cpp
SOURCES += captions/vbilut.cpp

# MPEG parsing stuff
HEADERS += mpeg/tspacket.h          mpeg/pespacket.h
HEADERS += mpeg/mpegtables.h        mpeg/atsctables.h
HEADERS += mpeg/dvbtables.h         mpeg/premieretables.h
HEADERS += mpeg/sctetables.h
HEADERS += mpeg/mpegstreamdata.h    mpeg/atscstreamdata.h
HEADERS += mpeg/dvbstreamdata.h     mpeg/scanstreamdata.h
HEADERS += mpeg/mpegdescriptors.h   mpeg/atscdescriptors.h
HEADERS += mpeg/sctedescriptors.h   mpeg/dvbdescriptors.h
HEADERS += mpeg/splicedescriptors.h
HEADERS += mpeg/dishdescriptors.h   mpeg/premieredescriptors.h
HEADERS += mpeg/atsc_huffman.h
HEADERS += mpeg/freesat_huffman.h
HEADERS += mpeg/iso6937tables.h
HEADERS += mpeg/tsstats.h           mpeg/streamlisteners.h
HEADERS += mpeg/H2645Parser.h mpeg/AVCParser.h mpeg/HEVCParser.h
HEADERS += mpeg/tablestatus.h
HEADERS += mpeg/tsstreamdata.h

SOURCES += mpeg/tspacket.cpp        mpeg/pespacket.cpp
SOURCES += mpeg/mpegtables.cpp      mpeg/atsctables.cpp
SOURCES += mpeg/dvbtables.cpp       mpeg/premieretables.cpp
SOURCES += mpeg/sctetables.cpp
SOURCES += mpeg/mpegstreamdata.cpp  mpeg/atscstreamdata.cpp
SOURCES += mpeg/dvbstreamdata.cpp   mpeg/scanstreamdata.cpp
SOURCES += mpeg/mpegdescriptors.cpp mpeg/atscdescriptors.cpp
SOURCES += mpeg/dvbdescriptors.cpp  mpeg/sctedescriptors.cpp
SOURCES += mpeg/splicedescriptors.cpp
SOURCES += mpeg/dishdescriptors.cpp mpeg/premieredescriptors.cpp
SOURCES += mpeg/atsc_huffman.cpp    mpeg/freesat_tables.cpp
SOURCES += mpeg/freesat_huffman.cpp
SOURCES += mpeg/iso6937tables.cpp
SOURCES += mpeg/H2645Parser.cpp mpeg/AVCParser.cpp mpeg/HEVCParser.cpp
SOURCES += mpeg/tablestatus.cpp
SOURCES += mpeg/tsstreamdata.cpp

# Channels, and the multiplexes that transmit them
HEADERS += frequencies.h            frequencytables.h
SOURCES += frequencies.cpp          frequencytables.cpp

HEADERS += channelutil.h            channelinfo.h
HEADERS += iptvtuningdata.h
SOURCES += channelutil.cpp          channelinfo.cpp
SOURCES += iptvtuningdata.cpp

HEADERS += dtvmultiplex.h
HEADERS += dtvconfparser.h          dtvconfparserhelpers.h
SOURCES += dtvmultiplex.cpp
SOURCES += dtvconfparser.cpp        dtvconfparserhelpers.cpp

HEADERS += channelscan/scaninfo.h   channelscan/channelimporter.h
HEADERS += channelscan/iptvchannelfetcher.h
SOURCES += channelscan/scaninfo.cpp channelscan/channelimporter.cpp
SOURCES += channelscan/iptvchannelfetcher.cpp

# subtitles: srt
HEADERS += captions/srtwriter.h
SOURCES += captions/srtwriter.cpp

inc.path = $${PREFIX}/include/mythtv/libmythtv
inc.files  = playgroup.h
inc.files += mythtvexp.h            metadataimagehelper.h
inc.files += mythavutil.h           mythframe.h

INSTALLS += inc

inc2.path = $${PREFIX}/include/mythtv/libmythtv/visualisations/goom
inc2.files  = visualisations/goom/filters.h
inc2.files += visualisations/goom/goomconfig.h
inc2.files += visualisations/goom/goom_core.h
inc2.files += visualisations/goom/goom_tools.h
inc2.files += visualisations/goom/graphic.h
inc2.files += visualisations/goom/ifs.h
inc2.files += visualisations/goom/lines.h
inc2.files += visualisations/goom/drawmethods.h
inc2.files += visualisations/goom/mmx.h
inc2.files += visualisations/goom/mathtools.h
inc2.files += visualisations/goom/tentacle3d.h
inc2.files += visualisations/goom/v3d.h

INSTALLS += inc2

#DVD stuff
DEPENDPATH  += ../../external/libmythdvdnav/
DEPENDPATH  += ../../external/libmythdvdnav/dvdread # for dvd_reader.h & dvd_input.h
INCLUDEPATH += ../../external/libmythdvdnav/dvdnav
INCLUDEPATH += ../../external/libmythdvdnav/dvdread

win32-msvc*|freebsd {
  INCLUDEPATH += ../../external/libmythdvdnav/dvdnav
  INCLUDEPATH += ../../external/libmythdvdnav/dvdread
} else {
  QMAKE_CXXFLAGS += -isystem ../../external/libmythdvdnav/dvdnav
  QMAKE_CXXFLAGS += -isystem ../../external/libmythdvdnav/dvdread
}

!win32-msvc*:POST_TARGETDEPS += ../../external/libmythdvdnav/libmythdvdnav-$${MYTH_LIB_EXT}

HEADERS += DVD/mythdvdbuffer.h
HEADERS += DVD/mythdvdcontext.h
HEADERS += DVD/mythdvdinfo.h
HEADERS += DVD/mythdvdstream.h
SOURCES += DVD/mythdvdbuffer.cpp
SOURCES += DVD/mythdvdcontext.cpp
SOURCES += DVD/mythdvdinfo.cpp
SOURCES += DVD/mythdvdstream.cpp
using_frontend {
    HEADERS += DVD/mythdvdplayer.h
    SOURCES += DVD/mythdvdplayer.cpp
    HEADERS += DVD/mythdvddecoder.h
    SOURCES += DVD/mythdvddecoder.cpp
}
LIBS += -L../../external/libmythdvdnav
LIBS += -lmythdvdnav-$$LIBVERSION

#Bluray stuff
HEADERS += Bluray/mythbdiowrapper.h
HEADERS += Bluray/mythbdbuffer.h
HEADERS += Bluray/mythbdinfo.h
HEADERS += Bluray/mythbdoverlay.h
SOURCES += Bluray/mythbdiowrapper.cpp
SOURCES += Bluray/mythbdbuffer.cpp
SOURCES += Bluray/mythbdinfo.cpp
SOURCES += Bluray/mythbdoverlay.cpp
using_frontend {
    HEADERS += Bluray/mythbdplayer.h
    SOURCES += Bluray/mythbdplayer.cpp
    HEADERS += Bluray/mythbddecoder.h
    SOURCES += Bluray/mythbddecoder.cpp
    HEADERS += Bluray/mythbdoverlayscreen.h
    SOURCES += Bluray/mythbdoverlayscreen.cpp
}
!using_system_libbluray {
    INCLUDEPATH += ../../external/libmythbluray/src
    DEPENDPATH += ../../external/libmythbluray
    LIBS += -L../../external/libmythbluray     -lmythbluray-$${LIBVERSION}
    !win32-msvc*:POST_TARGETDEPS += ../../external/libmythbluray/libmythbluray-$${MYTH_LIB_EXT}
} else {
    DEFINES += HAVE_LIBBLURAY
}
using_system_libbluray:mingw {
    LIBS += -lbluray
}

using_system_libbluray:android {
    LIBS += -lbluray -lxml2
}

#HLS stuff
HEADERS += HLS/httplivestream.h
SOURCES += HLS/httplivestream.cpp
HEADERS += HLS/httplivestreambuffer.h
SOURCES += HLS/httplivestreambuffer.cpp
HEADERS += HLS/m3u.h
SOURCES += HLS/m3u.cpp
using_libcrypto:LIBS    += -lcrypto

using_frontend {
    # Recording profile stuff
    HEADERS += profilegroup.h
    SOURCES += profilegroup.cpp

    # Video playback
    HEADERS += tv_play.h
    HEADERS += tvplaybackstate.h
    HEADERS += mythtvmenu.h
    HEADERS += mythtvactionutils.h
    HEADERS += mythplayer.h
    HEADERS += mythplayerstate.h
    HEADERS += mythplayeruibase.h
    HEADERS += mythplayerui.h
    HEADERS += mythplayereditorui.h
    HEADERS += mythplayervideoui.h
    HEADERS += mythplayercaptionsui.h
    HEADERS += mythplayeroverlayui.h
    HEADERS += mythvideoscantracker.h
    HEADERS += mythplayervisualiserui.h
    HEADERS += mythplayeravsync.h
    HEADERS += mythplayeraudioui.h
    HEADERS += audioplayer.h
    HEADERS += mythccextractorplayer.h
    HEADERS += captions/teletextextractorreader.h
    HEADERS += playercontext.h
    HEADERS += tv_play_win.h            deletemap.h
    HEADERS += mythcommflagplayer.h     commbreakmap.h
    HEADERS += mythpreviewplayer.h
    HEADERS += tvbrowsehelper.h
    HEADERS += mheg/netstream.h
    SOURCES += tv_play.cpp
    SOURCES += tvplaybackstate.cpp
    SOURCES += mythtvmenu.cpp
    SOURCES += mythplayer.cpp
    SOURCES += mythplayerstate.cpp
    SOURCES += mythplayeruibase.cpp
    SOURCES += mythplayerui.cpp
    SOURCES += mythplayereditorui.cpp
    SOURCES += mythplayervideoui.cpp
    SOURCES += mythplayercaptionsui.cpp
    SOURCES += mythplayeroverlayui.cpp
    SOURCES += mythvideoscantracker.cpp
    SOURCES += mythplayervisualiserui.cpp
    SOURCES += mythplayeravsync.cpp
    SOURCES += mythplayeraudioui.cpp
    SOURCES += audioplayer.cpp
    SOURCES += mythccextractorplayer.cpp
    SOURCES += captions/teletextextractorreader.cpp
    SOURCES += playercontext.cpp
    SOURCES += tv_play_win.cpp          deletemap.cpp
    SOURCES += mythcommflagplayer.cpp   commbreakmap.cpp
    SOURCES += mythpreviewplayer.cpp
    SOURCES += tvbrowsehelper.cpp
    SOURCES += mheg/netstream.cpp

    # Input/output
    HEADERS += io/mythiowrapper.h
    SOURCES += io/mythiowrapper.cpp

    win32-msvc*:SOURCES += ../../../platform/win32/msvc/src/posix/dirent.c

    # Text subtitle parser
    HEADERS += captions/textsubtitleparser.h
    SOURCES += captions/textsubtitleparser.cpp

    # A/V decoders
    HEADERS += decoders/decoderbase.h
    HEADERS += decoders/avformatdecoder.h
    HEADERS += decoders/mythcodeccontext.h
    HEADERS += decoders/mythdecoderthread.h
    SOURCES += decoders/decoderbase.cpp
    SOURCES += decoders/avformatdecoder.cpp
    SOURCES += decoders/mythcodeccontext.cpp
    SOURCES += decoders/mythdecoderthread.cpp

    using_libass: LIBS += -lass

    # On screen display (video output overlay)
    HEADERS += osd.h
    HEADERS += mythmediaoverlay.h
    HEADERS += mythcaptionsoverlay.h
    HEADERS += captions/teletextscreen.h
    HEADERS += captions/subtitlescreen.h
    HEADERS += overlays/mythnavigationoverlay.h
    HEADERS += overlays/mythchanneloverlay.h
    HEADERS += mheg/interactivescreen.h
    SOURCES += osd.cpp
    SOURCES += mythmediaoverlay.cpp
    SOURCES += mythcaptionsoverlay.cpp
    SOURCES += captions/teletextscreen.cpp
    SOURCES += captions/subtitlescreen.cpp
    SOURCES += overlays/mythnavigationoverlay.cpp
    SOURCES += overlays/mythchanneloverlay.cpp
    SOURCES += mheg/interactivescreen.cpp

    # Video output
    HEADERS += mythvideoout.h
    HEADERS += mythvideooutnull.h
    HEADERS += mythvideooutgpu.h
    HEADERS += mythvideogpu.h
    HEADERS += videobuffers.h
    HEADERS += jitterometer.h
    HEADERS += mythvideoprofile.h mythcodecid.h
    HEADERS += videoouttypes.h
    HEADERS += mythvideobounds.h
    HEADERS += mythvideocolourspace.h
    HEADERS += visualisations/videovisual.h
    HEADERS += visualisations/videovisualdefs.h
    HEADERS += visualisations/videovisualspectrum.h
    HEADERS += mythdeinterlacer.h
    HEADERS += mythinteropgpu.h
    SOURCES += mythvideoout.cpp
    SOURCES += mythvideooutnull.cpp
    SOURCES += mythvideooutgpu.cpp
    SOURCES += mythvideogpu.cpp
    SOURCES += videobuffers.cpp
    SOURCES += jitterometer.cpp
    SOURCES += mythvideoprofile.cpp mythcodecid.cpp
    SOURCES += mythvideobounds.cpp
    SOURCES += mythvideocolourspace.cpp
    SOURCES += visualisations/videovisual.cpp
    SOURCES += visualisations/videovisualspectrum.cpp
    SOURCES += mythdeinterlacer.cpp
    SOURCES += mythinteropgpu.cpp

    # Note - all OpenGL/EGL interop files are added under using_opengl...
    # They are however referenced, without ifdef guards, from the relevant
    # following files. It is essentially assumed that any given hardware
    # acceleration must have associated OpenGL support (e.g. there is no VDPAU
    # without a functioning NVidia driver)
    using_mmal {
        HEADERS += decoders/mythmmalcontext.h
        SOURCES += decoders/mythmmalcontext.cpp
        LIBS    += -L/opt/vc/lib -lmmal -lvcsm
        # Raspbian
        QMAKE_CXXFLAGS += -isystem /opt/vc/include
    }

    using_vdpau:using_x11 {
        HEADERS += decoders/mythvdpaucontext.h   decoders/mythvdpauhelper.h
        SOURCES += decoders/mythvdpaucontext.cpp decoders/mythvdpauhelper.cpp
        LIBS += -lvdpau
    }

    using_drm:using_qtprivateheaders {
        QT += gui-private
        QMAKE_CXXFLAGS += $${LIBDRM_CFLAGS}
        HEADERS += drm/mythvideodrm.h
        HEADERS += drm/mythvideodrmbuffer.h
        HEADERS += drm/mythvideodrmutils.h
        SOURCES += drm/mythvideodrm.cpp
        SOURCES += drm/mythvideodrmbuffer.cpp
        SOURCES += drm/mythvideodrmutils.cpp
    }

    using_vaapi {
        HEADERS += decoders/mythvaapicontext.h
        SOURCES += decoders/mythvaapicontext.cpp
        LIBS    += -lva -lva-x11 -lva-glx -lva-drm
    }

    using_nvdec {
        HEADERS += decoders/mythnvdeccontext.h
        SOURCES += decoders/mythnvdeccontext.cpp
        INCLUDEPATH += ../../external/nv-codec-headers/include
    }

    using_mediacodec {
        HEADERS += decoders/mythmediacodeccontext.h
        SOURCES += decoders/mythmediacodeccontext.cpp
    }

    HEADERS += decoders/mythdrmprimecontext.h
    SOURCES += decoders/mythdrmprimecontext.cpp

    using_vulkan {
        HEADERS += vulkan/mythvideovulkan.h
        HEADERS += vulkan/mythvideooutputvulkan.h
        HEADERS += vulkan/mythvideotexturevulkan.h
        HEADERS += vulkan/mythvideoshadersvulkan.h
        SOURCES += vulkan/mythvideovulkan.cpp
        SOURCES += vulkan/mythvideooutputvulkan.cpp
        SOURCES += vulkan/mythvideotexturevulkan.cpp
    }

    using_vulkan|using_opengl {
        HEADERS += visualisations/videovisualmonoscope.h
        SOURCES += visualisations/videovisualmonoscope.cpp

        using_opengl {
            HEADERS += visualisations/opengl/mythvisualmonoscopeopengl.h
            SOURCES += visualisations/opengl/mythvisualmonoscopeopengl.cpp
        }

        using_vulkan {
            HEADERS += visualisations/vulkan/mythvisualvulkan.h
            HEADERS += visualisations/vulkan/mythvisualcirclesvulkan.h
            HEADERS += visualisations/vulkan/mythvisualmonoscopevulkan.h
            SOURCES += visualisations/vulkan/mythvisualvulkan.cpp
            SOURCES += visualisations/vulkan/mythvisualcirclesvulkan.cpp
            SOURCES += visualisations/vulkan/mythvisualmonoscopevulkan.cpp
        }
    }

    using_opengl {
        HEADERS += opengl/mythopenglvideo.h
        HEADERS += opengl/mythvideooutopengl.h
        HEADERS += opengl/mythopenglvideoshaders.h
        HEADERS += opengl/mythopenglinterop.h
        HEADERS += opengl/mythvideotextureopengl.h
        HEADERS += opengl/mythopengltonemap.h
        HEADERS += opengl/mythopenglcomputeshaders.h
        HEADERS += visualisations/videovisualcircles.h
        SOURCES += opengl/mythopenglvideo.cpp
        SOURCES += opengl/mythvideooutopengl.cpp
        SOURCES += opengl/mythopenglinterop.cpp
        SOURCES += opengl/mythvideotextureopengl.cpp
        SOURCES += opengl/mythopengltonemap.cpp
        SOURCES += visualisations/videovisualcircles.cpp


        using_vaapi {
            HEADERS += opengl/mythvaapiinterop.h   opengl/mythvaapiglxinterop.h
            SOURCES += opengl/mythvaapiinterop.cpp opengl/mythvaapiglxinterop.cpp
        }

        using_vdpau:using_x11 {
            HEADERS += opengl/mythvdpauinterop.h
            SOURCES += opengl/mythvdpauinterop.cpp
        }

        using_nvdec {
            HEADERS += opengl/mythnvdecinterop.h
            SOURCES += opengl/mythnvdecinterop.cpp
        }

        using_mediacodec {
            # this may technically be egl...
            HEADERS += opengl/mythmediacodecinterop.h
            SOURCES += opengl/mythmediacodecinterop.cpp
        }

        macx:using_videotoolbox {
            HEADERS += opengl/mythvtbinterop.h
            SOURCES += opengl/mythvtbinterop.cpp
        }

        using_egl {
            HEADERS += opengl/mythegldefs.h
            HEADERS += opengl/mythegldmabuf.h
            HEADERS += opengl/mythdrmprimeinterop.h
            SOURCES += opengl/mythdrmprimeinterop.cpp
            SOURCES += opengl/mythegldmabuf.cpp

            using_mmal {
                HEADERS += opengl/mythmmalinterop.h
                SOURCES += opengl/mythmmalinterop.cpp
            }

            using_vaapi {
                HEADERS += opengl/mythvaapidrminterop.h
                SOURCES += opengl/mythvaapidrminterop.cpp
            }
        }

        !win32-msvc* {
            # Goom
            HEADERS += visualisations/goom/filters.h
            HEADERS += visualisations/goom/goomconfig.h
            HEADERS += visualisations/goom/goom_core.h
            HEADERS += visualisations/goom/goom_tools.h
            HEADERS += visualisations/goom/graphic.h
            HEADERS += visualisations/goom/ifs.h
            HEADERS += visualisations/goom/lines.h
            HEADERS += visualisations/goom/drawmethods.h
            HEADERS += visualisations/goom/mmx.h
            HEADERS += visualisations/goom/mathtools.h
            HEADERS += visualisations/goom/surf3d.h
            HEADERS += visualisations/goom/tentacle3d.h
            HEADERS += visualisations/goom/v3d.h
            HEADERS += visualisations/goom/zoom_filters.h
            HEADERS += visualisations/videovisualgoom.h

            SOURCES += visualisations/goom/filters.cpp
            SOURCES += visualisations/goom/goom_core.cpp
            SOURCES += visualisations/goom/graphic.cpp
            SOURCES += visualisations/goom/tentacle3d.cpp
            SOURCES += visualisations/goom/ifs.cpp
            SOURCES += visualisations/goom/ifs_display.cpp
            SOURCES += visualisations/goom/lines.cpp
            SOURCES += visualisations/goom/surf3d.cpp
            SOURCES += visualisations/goom/zoom_filter_mmx.cpp
            SOURCES += visualisations/goom/zoom_filter_xmmx.cpp
            SOURCES += visualisations/videovisualgoom.cpp
        }
    }

    # Misc. frontend
    HEADERS += DetectLetterbox.h
    SOURCES += DetectLetterbox.cpp

    using_libdns_sd {
        !macx: LIBS += -ldns_sd
        using_libcrypto {
            HEADERS += AirPlay/mythairplayserver.h
            SOURCES += AirPlay/mythairplayserver.cpp
            HEADERS += AirPlay/mythraopdevice.h   AirPlay/mythraopconnection.h
            SOURCES += AirPlay/mythraopdevice.cpp AirPlay/mythraopconnection.cpp
        }
    }

    using_mheg {
        # DSMCC stuff
        HEADERS += mheg/dsmcc.h             mheg/dsmcccache.h
        HEADERS += mheg/dsmccbiop.h         mheg/dsmccobjcarousel.h
        HEADERS += mheg/dsmccreceiver.h
        SOURCES += mheg/dsmcc.cpp           mheg/dsmcccache.cpp
        SOURCES += mheg/dsmccbiop.cpp       mheg/dsmccobjcarousel.cpp

         # MHEG interaction channel
        HEADERS += mheg/mhegic.h
        SOURCES += mheg/mhegic.cpp

        # MHEG/MHI stuff
        HEADERS += mheg/interactivetv.h     mheg/mhi.h
        SOURCES += mheg/interactivetv.cpp   mheg/mhi.cpp
    }

    using_v4l2 {
        HEADERS += decoders/mythv4l2m2mcontext.h
        SOURCES += decoders/mythv4l2m2mcontext.cpp
    }
}

if(using_backend|using_frontend):using_v4l2 {
    HEADERS += v4l2util.h
    SOURCES += v4l2util.cpp
}

using_backend {
    # Channel stuff
    HEADERS += recorders/channelbase.h
    HEADERS += recorders/dtvchannel.h
    HEADERS += recorders/signalmonitor.h
    HEADERS += recorders/dtvsignalmonitor.h
    HEADERS += recorders/scriptsignalmonitor.h
    SOURCES += recorders/channelbase.cpp
    SOURCES += recorders/dtvchannel.cpp
    SOURCES += recorders/signalmonitor.cpp
    SOURCES += recorders/dtvsignalmonitor.cpp

    HEADERS += inputinfo.h
    SOURCES += inputinfo.cpp

    # Channel scanner stuff
    HEADERS += scanwizard.h
    SOURCES += scanwizard.cpp

    HEADERS += channelscan/channelscan_sm.h
    HEADERS += channelscan/channelscanner.h
    HEADERS += channelscan/channelscanner_gui.h
    HEADERS += channelscan/channelscanner_gui_scan_pane.h
    HEADERS += channelscan/channelscanner_cli.h
    HEADERS += channelscan/channelscanner_web.h
    HEADERS += channelscan/frequencytablesetting.h
    HEADERS += channelscan/inputselectorsetting.h
    HEADERS += channelscan/channelscanmiscsettings.h
    HEADERS += channelscan/modulationsetting.h
    HEADERS += channelscan/multiplexsetting.h
    HEADERS += channelscan/paneanalog.h
    HEADERS += channelscan/paneatsc.h
    HEADERS += channelscan/panedvbc.h
    HEADERS += channelscan/panedvbs.h
    HEADERS += channelscan/panedvbs2.h
    HEADERS += channelscan/panedvbt.h
    HEADERS += channelscan/panedvbt2.h
    HEADERS += channelscan/panedvbutilsimport.h
    HEADERS += channelscan/panesingle.h
    HEADERS += channelscan/scanmonitor.h
    HEADERS += channelscan/scanwizardconfig.h

    SOURCES += channelscan/channelscan_sm.cpp
    SOURCES += channelscan/channelscanner.cpp
    SOURCES += channelscan/channelscanner_gui.cpp
    SOURCES += channelscan/channelscanner_gui_scan_pane.cpp
    SOURCES += channelscan/channelscanner_cli.cpp
    SOURCES += channelscan/channelscanner_web.cpp
    SOURCES += channelscan/frequencytablesetting.cpp
    SOURCES += channelscan/inputselectorsetting.cpp
    SOURCES += channelscan/multiplexsetting.cpp
    SOURCES += channelscan/paneanalog.cpp
    SOURCES += channelscan/scanmonitor.cpp
    SOURCES += channelscan/scanwizardconfig.cpp

#if !defined( _WIN32 )
    HEADERS += channelscan/externrecscanner.h
    SOURCES += channelscan/externrecscanner.cpp
#endif

    # EIT stuff
    HEADERS += eithelper.h                 eitscanner.h
    HEADERS += eitfixup.h                  eitcache.h
    SOURCES += eithelper.cpp               eitscanner.cpp
    SOURCES += eitfixup.cpp                eitcache.cpp

    # non-EIT EPG stuff
    HEADERS += programdata.h
    SOURCES += programdata.cpp

    # TVRec stuff
    HEADERS += tv_rec.h                    recordingquality.h
    SOURCES += tv_rec.cpp                  recordingquality.cpp

    # Recorder base and util classes
    HEADERS += recorders/recorderbase.h
    HEADERS += recorders/DeviceReadBuffer.h
    HEADERS += recorders/dtvrecorder.h
    SOURCES += recorders/recorderbase.cpp
    SOURCES += recorders/DeviceReadBuffer.cpp
    SOURCES += recorders/dtvrecorder.cpp

    # Import recorder
    HEADERS += recorders/importrecorder.h
    SOURCES += recorders/importrecorder.cpp

    HEADERS += recorders/audioinput.h

    SOURCES += recorders/audioinput.cpp
    using_alsa {
        HEADERS += recorders/audioinputalsa.h
        SOURCES += recorders/audioinputalsa.cpp
    }
    using_oss {
        HEADERS += recorders/audioinputoss.h
        SOURCES += recorders/audioinputoss.cpp
    }

    # Support for Video4Linux devices

    !mingw:!win32-msvc* {
        HEADERS += recorders/v4lrecorder.h
        SOURCES += recorders/v4lrecorder.cpp
    }

    using_v4l2 {
        HEADERS += recorders/v4lchannel.h
        HEADERS += recorders/analogsignalmonitor.h
        SOURCES += recorders/v4lchannel.cpp
        SOURCES += recorders/analogsignalmonitor.cpp

        HEADERS += recorders/v4l2encrecorder.h
        SOURCES += recorders/v4l2encrecorder.cpp
        HEADERS += recorders/v4l2encstreamhandler.h
        SOURCES += recorders/v4l2encstreamhandler.cpp
        HEADERS += recorders/v4l2encsignalmonitor.h
        SOURCES += recorders/v4l2encsignalmonitor.cpp

        HEADERS += recorders/mpegrecorder.h
        SOURCES += recorders/mpegrecorder.cpp
    }

    # Support for cable boxes that provide Firewire out
    using_firewire {
        HEADERS += recorders/firewirechannel.h
        HEADERS += recorders/firewirerecorder.h
        HEADERS += recorders/firewiresignalmonitor.h
        HEADERS += recorders/firewiredevice.h
        HEADERS += recorders/avcinfo.h
        SOURCES += recorders/firewirechannel.cpp
        SOURCES += recorders/firewirerecorder.cpp
        SOURCES += recorders/firewiresignalmonitor.cpp
        SOURCES += recorders/firewiredevice.cpp
        SOURCES += recorders/avcinfo.cpp

        macx {
            HEADERS += recorders/darwinfirewiredevice.h
            HEADERS += recorders/darwinavcinfo.h
            SOURCES += recorders/darwinfirewiredevice.cpp
            SOURCES += recorders/darwinavcinfo.cpp
        }

        !macx {
            HEADERS += recorders/linuxfirewiredevice.h
            HEADERS += recorders/linuxavcinfo.h
            SOURCES += recorders/linuxfirewiredevice.cpp
            SOURCES += recorders/linuxavcinfo.cpp
        }
    }

    # Support for RTP/UDP streams
    HEADERS += recorders/cetonrtsp.h
    HEADERS += recorders/iptvchannel.h
    HEADERS += recorders/iptvrecorder.h
    HEADERS += recorders/iptvsignalmonitor.h
    HEADERS += recorders/iptvstreamhandler.h
    HEADERS *= recorders/streamhandler.h

    HEADERS += recorders/rtp/udppacket.h
    HEADERS += recorders/rtp/udppacketbuffer.h
    HEADERS += recorders/rtp/packetbuffer.h
    HEADERS += recorders/rtp/rtppacketbuffer.h
    HEADERS += recorders/rtp/rtpdatapacket.h
    HEADERS += recorders/rtp/rtpfecpacket.h
    HEADERS += recorders/rtp/rtcpdatapacket.h

    SOURCES += recorders/cetonrtsp.cpp
    SOURCES += recorders/iptvchannel.cpp
    SOURCES += recorders/iptvrecorder.cpp
    SOURCES += recorders/iptvsignalmonitor.cpp
    SOURCES += recorders/iptvstreamhandler.cpp
    SOURCES *= recorders/streamhandler.cpp

    SOURCES += recorders/rtp/packetbuffer.cpp
    SOURCES += recorders/rtp/rtpdatapacket.cpp
    SOURCES += recorders/rtp/rtppacketbuffer.cpp

    # Support for HTTP TS streams
    HEADERS += recorders/httptsstreamhandler.h
    SOURCES += recorders/httptsstreamhandler.cpp

    # Suppport for HLS recorder
    HEADERS += recorders/hlsstreamhandler.h
    SOURCES += recorders/hlsstreamhandler.cpp

    HEADERS += recorders/HLS/HLSPlaylistWorker.h
    HEADERS += recorders/HLS/HLSReader.h
    HEADERS += recorders/HLS/HLSSegment.h
    HEADERS += recorders/HLS/HLSStream.h
    HEADERS += recorders/HLS/HLSStreamWorker.h

    SOURCES += recorders/HLS/HLSPlaylistWorker.cpp
    SOURCES += recorders/HLS/HLSReader.cpp
    SOURCES += recorders/HLS/HLSSegment.cpp
    SOURCES += recorders/HLS/HLSStream.cpp
    SOURCES += recorders/HLS/HLSStreamWorker.cpp


    # Support for HDHomeRun box
    using_hdhomerun {
        # MythTV HDHomeRun glue

        HEADERS += recorders/hdhrsignalmonitor.h
        HEADERS += recorders/hdhrchannel.h
        HEADERS += recorders/hdhrrecorder.h
        HEADERS += recorders/hdhrstreamhandler.h

        SOURCES += recorders/hdhrsignalmonitor.cpp
        SOURCES += recorders/hdhrchannel.cpp
        SOURCES += recorders/hdhrrecorder.cpp
        SOURCES += recorders/hdhrstreamhandler.cpp

        # HDHomeRun channel list import
        HEADERS += channelscan/hdhrchannelfetcher.h
        SOURCES += channelscan/hdhrchannelfetcher.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp

        DEFINES += HDHOMERUN_HEADERFILE=\\\"$${HDHOMERUN_PREFIX}hdhomerun.h\\\"
        DEFINES += HDHOMERUN_VERSION=$${HDHOMERUN_VERSION}
    }

    # Support for Sat>IP
    using_satip {
        HEADERS += recorders/satiputils.h
        HEADERS += recorders/satipchannel.h
        HEADERS += recorders/satipstreamhandler.h
        HEADERS += recorders/satipsignalmonitor.h
        HEADERS += recorders/satiprtsp.h
        HEADERS += recorders/satiprecorder.h
        HEADERS += recorders/satiprtcppacket.h

        SOURCES += recorders/satiputils.cpp
        SOURCES += recorders/satipchannel.cpp
        SOURCES += recorders/satipstreamhandler.cpp
        SOURCES += recorders/satipsignalmonitor.cpp
        SOURCES += recorders/satiprtsp.cpp
        SOURCES += recorders/satiprecorder.cpp
        SOURCES += recorders/satiprtcppacket.cpp
    }

    # Support for VBox
    using_vbox {
        HEADERS += recorders/vboxutils.h
        HEADERS += channelscan/vboxchannelfetcher.h

        SOURCES += recorders/vboxutils.cpp
        SOURCES += channelscan/vboxchannelfetcher.cpp
    }

    # Support for Ceton
    using_ceton {
        # MythTV Ceton glue
        HEADERS += recorders/cetonsignalmonitor.h
        HEADERS += recorders/cetonchannel.h
        HEADERS += recorders/cetonrecorder.h
        HEADERS += recorders/cetonstreamhandler.h

        SOURCES += recorders/cetonsignalmonitor.cpp
        SOURCES += recorders/cetonchannel.cpp
        SOURCES += recorders/cetonrecorder.cpp
        SOURCES += recorders/cetonstreamhandler.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp
    }

    # External recorder
    HEADERS += recorders/ExternalChannel.h
    SOURCES += recorders/ExternalChannel.cpp
    HEADERS += recorders/ExternalRecChannelFetcher.h
    SOURCES += recorders/ExternalRecChannelFetcher.cpp
    HEADERS += recorders/ExternalRecorder.h
    SOURCES += recorders/ExternalRecorder.cpp
    HEADERS += recorders/ExternalStreamHandler.h
    SOURCES += recorders/ExternalStreamHandler.cpp
    HEADERS += recorders/ExternalSignalMonitor.h
    SOURCES += recorders/ExternalSignalMonitor.cpp

    # Support for Linux DVB drivers
    using_dvb {
        # Basic DVB types
        HEADERS += recorders/dvbtypes.h
        SOURCES += recorders/dvbtypes.cpp

        # Channel stuff
        HEADERS += recorders/dvbchannel.h
        HEADERS += recorders/dvbsignalmonitor.h
        HEADERS += recorders/dvbcam.h
        SOURCES += recorders/dvbchannel.cpp
        SOURCES += recorders/dvbsignalmonitor.cpp
        SOURCES += recorders/dvbcam.cpp

        # DVB Recorder
        HEADERS += recorders/dvbrecorder.h
        HEADERS += recorders/dvbstreamhandler.h
        SOURCES += recorders/dvbrecorder.cpp
        SOURCES += recorders/dvbstreamhandler.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp

        # Misc
        HEADERS += recorders/dvbdev/dvbci.h
        SOURCES += recorders/dvbdev/dvbci.cpp
    }

    using_asi {
        # Channel stuff
        HEADERS += recorders/asichannel.h
        HEADERS += recorders/asisignalmonitor.h
        SOURCES += recorders/asichannel.cpp
        SOURCES += recorders/asisignalmonitor.cpp

        # ASI Recorder
        HEADERS += recorders/asirecorder.h
        HEADERS += recorders/asistreamhandler.h
        SOURCES += recorders/asirecorder.cpp
        SOURCES += recorders/asistreamhandler.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp
    }
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

mingw || win32-msvc* {

    #HEADERS += videoout_d3d.h
    #SOURCES += videoout_d3d.cpp

    using_dxva2: HEADERS += dxva2decoder.h
    using_dxva2: SOURCES += dxva2decoder.cpp

    LIBS += -lws2_32 -lfreetype -lz
}

win32-msvc* {
  LIBS += -lzlib
  QMAKE_CXXFLAGS += "/FI compat.h"
  DEFINES += HAVE_STRUCT_TIMESPEC
}


# Dependencies and required libraries
# Have them at the end in order to properly resolve on mingw platform
# where the order is of significance
LIBS += -L../libmyth
LIBS += -L../../external/FFmpeg/libswresample -lmythswresample
LIBS += -L../../external/FFmpeg/libavutil
LIBS += -L../../external/FFmpeg/libavcodec
LIBS += -L../../external/FFmpeg/libavformat
LIBS += -L../../external/FFmpeg/libswscale
LIBS += -L../../external/FFmpeg/libpostproc
LIBS += -L../../external/FFmpeg/libavfilter
LIBS += -L../libmythui -L../libmythupnp
LIBS += -L../libmythbase
LIBS += -lmyth-$$LIBVERSION
LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythpostproc
LIBS += -lmythavfilter
LIBS += -lmythui-$$LIBVERSION       -lmythupnp-$$LIBVERSION
LIBS += -lmythbase-$$LIBVERSION
using_mheg: LIBS += -L../libmythfreemheg -lmythfreemheg-$$LIBVERSION
using_hdhomerun: LIBS += -lhdhomerun
LIBS += $$EXTRA_LIBS $$QMAKE_LIBS_DYNLOAD

!mingw || win32-msvc* {
    POST_TARGETDEPS += ../libmyth/libmyth-$${MYTH_SHLIB_EXT}
    POST_TARGETDEPS += ../../external/FFmpeg/libswresample/$$avLibName(swresample)
    POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
    POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
    POST_TARGETDEPS += ../../external/FFmpeg/libavformat/$$avLibName(avformat)
    POST_TARGETDEPS += ../../external/FFmpeg/libswscale/$$avLibName(swscale)
    POST_TARGETDEPS += ../../external/FFmpeg/libpostproc/$$avLibName(postproc)
    POST_TARGETDEPS += ../../external/FFmpeg/libavfilter/$$avLibName(avfilter)

    using_mheg: POST_TARGETDEPS += ../libmythfreemheg/libmythfreemheg-$${MYTH_SHLIB_EXT}
}

INCLUDEPATH += $$POSTINC

include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
DEFINES += MTV_API

test_clean.commands = -cd test/ && $(MAKE) -f Makefile clean
clean.depends = test_clean
QMAKE_EXTRA_TARGETS += test_clean clean
test_distclean.commands = -cd test/ && $(MAKE) -f Makefile distclean
distclean.depends = test_distclean
QMAKE_EXTRA_TARGETS += test_distclean distclean
