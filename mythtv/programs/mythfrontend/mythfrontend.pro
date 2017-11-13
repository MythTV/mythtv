include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql script
mingw | win32-msvc* {
   # script debugger currently only enabled for WIN32 builds
   QT += scripttools
}
contains(QT_VERSION, ^4\\.[0-9]\\..*) {
QT += webkit
using_qtdbus: CONFIG += qdbus
}
contains(QT_VERSION, ^5\\.[0-9]\\..*):using_qtwebkit {
QT += widgets
QT += webkitwidgets
using_qtdbus: QT += dbus
}

TEMPLATE = app
CONFIG += thread
TARGET = mythfrontend
target.path = $${PREFIX}/bin
INSTALLS = target

setting.path = $${PREFIX}/share/mythtv/
setting.files += MFEXML_scpd.xml
setting.extra = -ldconfig

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += playbackbox.h viewscheduled.h globalsettings.h audiogeneralsettings.h
HEADERS += manualschedule.h programrecpriority.h channelrecpriority.h
HEADERS += statusbox.h networkcontrol.h custompriority.h
HEADERS += mediarenderer.h mythfexml.h playbackboxlistitem.h
HEADERS += exitprompt.h
HEADERS += action.h mythcontrols.h keybindings.h keygrabber.h
HEADERS += progfind.h guidegrid.h customedit.h
HEADERS += schedulecommon.h scheduleeditor.h
HEADERS += backendconnectionmanager.h   programinfocache.h
HEADERS += proglist.h                   proglist_helpers.h
HEADERS += playbackboxhelper.h          viewschedulediff.h
HEADERS += themechooser.h               setupwizard_general.h
HEADERS += setupwizard_audio.h          setupwizard_video.h
HEADERS += grabbersettings.h            editvideometadata.h
HEADERS += videofileassoc.h             videometadatasettings.h
HEADERS += videoplayercommand.h         videopopups.h
HEADERS += videofilter.h                videolist.h
HEADERS += videoplayersettings.h        videodlg.h
HEADERS += videoglobalsettings.h        upnpscanner.h
HEADERS += commandlineparser.h          idlescreen.h
HEADERS += gallerythumbview.h           galleryslideview.h
HEADERS += galleryconfig.h              galleryviews.h
HEADERS += galleryslide.h               gallerytransitions.h
HEADERS += galleryinfo.h                prevreclist.h

SOURCES += main.cpp playbackbox.cpp viewscheduled.cpp audiogeneralsettings.cpp
SOURCES += globalsettings.cpp manualschedule.cpp programrecpriority.cpp
SOURCES += channelrecpriority.cpp statusbox.cpp networkcontrol.cpp
SOURCES += mediarenderer.cpp mythfexml.cpp playbackboxlistitem.cpp
SOURCES += custompriority.cpp exitprompt.cpp
SOURCES += action.cpp actionset.cpp  mythcontrols.cpp keybindings.cpp
SOURCES += keygrabber.cpp progfind.cpp guidegrid.cpp
SOURCES += customedit.cpp schedulecommon.cpp scheduleeditor.cpp
SOURCES += backendconnectionmanager.cpp programinfocache.cpp
SOURCES += proglist.cpp                 proglist_helpers.cpp
SOURCES += playbackboxhelper.cpp        viewschedulediff.cpp
SOURCES += themechooser.cpp             setupwizard_general.cpp
SOURCES += setupwizard_audio.cpp        setupwizard_video.cpp
SOURCES += grabbersettings.cpp          editvideometadata.cpp
SOURCES += videofileassoc.cpp           videometadatasettings.cpp
SOURCES += videoplayercommand.cpp       videopopups.cpp
SOURCES += videofilter.cpp              videolist.cpp
SOURCES += videoplayersettings.cpp      videodlg.cpp
SOURCES += videoglobalsettings.cpp      upnpscanner.cpp
SOURCES += commandlineparser.cpp        idlescreen.cpp
SOURCES += gallerythumbview.cpp         galleryslideview.cpp
SOURCES += galleryconfig.cpp            galleryviews.cpp
SOURCES += galleryslide.cpp             gallerytransitions.cpp
SOURCES += galleryinfo.cpp              prevreclist.cpp

HEADERS += serviceHosts/frontendServiceHost.h
HEADERS += services/frontend.h
SOURCES += services/frontend.cpp

HEADERS += progdetails.h proginfolist.h
SOURCES += progdetails.cpp proginfolist.cpp

macx {
    mac_bundle {
        CONFIG -= console  # Force behaviour of producing .app bundle
        RC_FILE += mythfrontend.icns
        QMAKE_POST_LINK = ../../contrib/OSX/build/makebundle.sh mythfrontend.app
    }

    # OS X has no ldconfig
    setting.extra -= -ldconfig
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}

win32 : !debug {
    # To hide the window that contains logging output:
    CONFIG -= console
    DEFINES += WINDOWS_CLOSE_CONSOLE
}

using_x11:DEFINES += USING_X11
using_xv:DEFINES += USING_XV
using_xrandr:DEFINES += USING_XRANDR
using_opengl:QT += opengl
using_opengl:DEFINES += USING_OPENGL
using_opengl_video:DEFINES += USING_OPENGL_VIDEO
using_vdpau:DEFINES += USING_VDPAU
using_vaapi:using_opengl_video:DEFINES += USING_GLVAAPI

using_pulse:DEFINES += USING_PULSE
using_pulseoutput: DEFINES += USING_PULSEOUTPUT
using_alsa:DEFINES += USING_ALSA
using_jack:DEFINES += USING_JACK
using_oss: DEFINES += USING_OSS
using_openmax: DEFINES += USING_OPENMAX
macx:      DEFINES += USING_COREAUDIO
using_libdns_sd {
    DEFINES += USING_LIBDNS_SD
    using_libcrypto: DEFINES += USING_AIRPLAY
}

android {
    QT += androidextras

    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libtag.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libexiv2.13.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libfreetype.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)mariadb/libmariadb.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libmythavutil.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libmythpostproc.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libmythswresample.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libmythswscale.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libmythavcodec.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIBCOMMON)libmythavformat.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmythbase-0.28.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmythui-0.28.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmythservicecontracts-0.28.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmythupnp-0.28.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmyth-0.28.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmythtv-0.28.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmythmetadata-0.28.so
    ANDROID_EXTRA_LIBS += $$(MYTHINSTALLLIB)libmythprotoserver-0.28.so

    ANDROID_PACKAGE_SOURCE_DIR += $$(MYTHPACKAGEBASE)/android-package-source
}

using_openmax {
    contains( HAVE_OPENMAX_BROADCOM, yes ) {
        using_opengl {
            # For raspberry Pi Raspbian Stretch
            exists(/opt/vc/lib/libbrcmEGL.so) {
                DEFINES += USING_OPENGLES
                # For raspberry pi raspbian
                QMAKE_RPATHDIR += $${RUNPREFIX}/share/mythtv/lib
                createlinks.path = $${PREFIX}/share/mythtv/lib
                createlinks.extra = ln -fs /opt/vc/lib/libbrcmEGL.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libEGL.so.1.0.0 ;
                createlinks.extra += ln -fs /opt/vc/lib/libbrcmEGL.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libEGL.so.1 ;
                createlinks.extra += ln -fs /opt/vc/lib/libbrcmGLESv2.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libGLESv2.so.2.0.0 ;
                createlinks.extra += ln -fs /opt/vc/lib/libbrcmGLESv2.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libGLESv2.so.2 ;
                INSTALLS += createlinks
            } else {
                # For raspberry Pi Raspbian pre-stretch
                exists(/opt/vc/lib/libEGL.so) {
                    DEFINES += USING_OPENGLES
                    # For raspberry pi raspbian
                    QMAKE_RPATHDIR += $${RUNPREFIX}/share/mythtv/lib
                    createlinks.path = $${PREFIX}/share/mythtv/lib
                    createlinks.extra = ln -fs /opt/vc/lib/libEGL.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libEGL.so.1.0.0 ;
                    createlinks.extra += ln -fs /opt/vc/lib/libEGL.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libEGL.so.1 ;
                    createlinks.extra += ln -fs /opt/vc/lib/libGLESv2.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libGLESv2.so.2.0.0 ;
                    createlinks.extra += ln -fs /opt/vc/lib/libGLESv2.so $(INSTALL_ROOT)/$${PREFIX}/share/mythtv/lib/libGLESv2.so.2 ;
                    INSTALLS += createlinks
                }
            }
        } else {
            # For raspberry pi ubuntu
            exists(/usr/lib/arm-linux-gnueabihf/mesa-egl/libEGL.so) {
                QMAKE_RPATHDIR += /usr/lib/arm-linux-gnueabihf/mesa-egl
            }
        }
    }
}
