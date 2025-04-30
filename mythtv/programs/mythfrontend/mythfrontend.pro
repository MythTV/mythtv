include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql widgets
using_qtscript: QT += script
mingw | win32-msvc* {
   # script debugger currently only enabled for WIN32 builds
   QT += scripttools
}
using_qtwebengine {
    equals(QT_MAJOR_VERSION, 6) {
        QT += webenginequick
    } else {
        QT += webengine
    }
    QT += webenginewidgets
}
android: QT += androidextras
using_qtdbus: QT += dbus

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
HEADERS += mythfrontend_commandlineparser.h          idlescreen.h
HEADERS += gallerythumbview.h           galleryslideview.h
HEADERS += galleryconfig.h              galleryviews.h
HEADERS += galleryslide.h               gallerytransitions.h
HEADERS += galleryinfo.h                prevreclist.h
HEADERS += settingshelper.h

SOURCES += mythfrontend.cpp playbackbox.cpp viewscheduled.cpp audiogeneralsettings.cpp
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
SOURCES += mythfrontend_commandlineparser.cpp        idlescreen.cpp
SOURCES += gallerythumbview.cpp         galleryslideview.cpp
SOURCES += galleryconfig.cpp            galleryviews.cpp
SOURCES += galleryslide.cpp             gallerytransitions.cpp
SOURCES += galleryinfo.cpp              prevreclist.cpp

HEADERS += servicecontracts/datacontracthelper.h
HEADERS += servicecontracts/frontendActionList.h
HEADERS += servicecontracts/frontendServices.h
HEADERS += servicecontracts/frontendStatus.h
SOURCES += servicecontracts/service.cpp
HEADERS += servicecontracts/service.h

HEADERS += serviceHosts/frontendServiceHost.h
SOURCES += serviceHosts/servicehost.cpp
HEADERS += serviceHosts/servicehost.h
SOURCES += serviceHosts/wsdl.cpp
HEADERS += serviceHosts/wsdl.h
SOURCES += serviceHosts/xsd.cpp
HEADERS += serviceHosts/xsd.h

HEADERS += services/frontend.h
SOURCES += services/frontend.cpp

HEADERS += services/mythfrontendservice.h
SOURCES += services/mythfrontendservice.cpp

HEADERS += progdetails.h proginfolist.h
SOURCES += progdetails.cpp proginfolist.cpp

HEADERS += playbackstate.h
SOURCES += playbackstate.cpp

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

android {
    message( myth android lib include $$(ANDROID_QT_DEPENDENCY_INCLUDE) )
    include ( $$(ANDROID_QT_DEPENDENCY_INCLUDE) )
}
