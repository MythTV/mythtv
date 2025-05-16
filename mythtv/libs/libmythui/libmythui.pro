include ( ../../settings.pro )

QT += xml sql network widgets
using_opengl: QT += opengl
using_qtwebengine: QT += webenginewidgets quick
android: QT += androidextras

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH += ../.. ../

LIBS += -L../libmythbase -lmythbase-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS  = mythmainwindowprivate.h mythmainwindow.h mythpainter.h mythimage.h mythrect.h
HEADERS += mythpainterwindow.h mythpainterwindowqt.h
HEADERS += mythuithemecache.h
HEADERS += mythuithemehelper.h
HEADERS += mythuilocation.h
HEADERS += mythuiscreenbounds.h
HEADERS += myththemebase.h
HEADERS += mythpainter_qt.h mythuihelper.h
HEADERS += mythpaintergpu.h
HEADERS += mythscreenstack.h mythgesture.h mythuitype.h mythscreentype.h
HEADERS += mythuiimage.h mythuitext.h mythuistatetype.h  xmlparsebase.h
HEADERS += mythuibutton.h myththemedmenu.h mythdialogbox.h
HEADERS += mythuiclock.h mythuitextedit.h mythprogressdialog.h mythuispinbox.h
HEADERS += mythuicheckbox.h mythuibuttonlist.h mythuigroup.h
HEADERS += mythuiprogressbar.h mythuifilebrowser.h
HEADERS += mythscreensaver.h
HEADERS += x11colors.h
HEADERS += themeinfo.h mythdisplaymode.h
HEADERS += mythgenerictree.h mythuibuttontree.h mythuiutils.h
HEADERS += mythvirtualkeyboard.h mythuishape.h mythuiguidegrid.h
HEADERS += mythrender_base.h mythfontmanager.h mythuieditbar.h
HEADERS += mythdisplay.h mythuivideo.h mythudplistener.h
HEADERS += mythuiexp.h mythuisimpletext.h mythuistatetracker.h
HEADERS += mythuianimation.h mythuiscrollbar.h
HEADERS += mythnotificationcenter.h mythnotificationcenter_private.h
HEADERS += mythuicomposite.h mythnotification.h
HEADERS += mythedid.h
HEADERS += mythhdr.h
HEADERS += mythvrr.h
HEADERS += mythcolourspace.h
HEADERS += devices/mythinputdevicehandler.h
HEADERS += mythuiprocedural.h
HEADERS += dbsettings.h
HEADERS += guistartup.h
HEADERS += langsettings.h
HEADERS += mediamonitor.h
HEADERS += mythterminal.h
HEADERS += rawsettingseditor.h
HEADERS += schemawizard.h
HEADERS += standardsettings.h
HEADERS += storagegroupeditor.h

SOURCES  = mythmainwindowprivate.cpp mythmainwindow.cpp mythpainter.cpp mythimage.cpp mythrect.cpp
SOURCES += mythpainterwindow.cpp mythpainterwindowqt.cpp
SOURCES += mythuithemecache.cpp
SOURCES += mythuithemehelper.cpp
SOURCES += mythuilocation.cpp
SOURCES += mythuiscreenbounds.cpp
SOURCES += myththemebase.cpp
SOURCES += mythrender.cpp
SOURCES += mythpainter_qt.cpp xmlparsebase.cpp mythuihelper.cpp
SOURCES += mythpaintergpu.cpp
SOURCES += mythscreenstack.cpp mythgesture.cpp mythuitype.cpp mythscreentype.cpp
SOURCES += mythuiimage.cpp mythuitext.cpp mythuifilebrowser.cpp
SOURCES += mythuistatetype.cpp mythfontproperties.cpp
SOURCES += mythuibutton.cpp myththemedmenu.cpp mythdialogbox.cpp
SOURCES += mythuiclock.cpp mythuitextedit.cpp mythprogressdialog.cpp
SOURCES += mythuispinbox.cpp mythuicheckbox.cpp mythuibuttonlist.cpp
SOURCES += mythuigroup.cpp mythuiprogressbar.cpp
SOURCES += mythscreensaver.cpp
SOURCES += x11colors.cpp
SOURCES += themeinfo.cpp mythdisplaymode.cpp
SOURCES += mythgenerictree.cpp mythuibuttontree.cpp mythuiutils.cpp
SOURCES += mythvirtualkeyboard.cpp mythuishape.cpp mythuiguidegrid.cpp
SOURCES += mythfontmanager.cpp mythuieditbar.cpp
SOURCES += mythdisplay.cpp mythuivideo.cpp mythudplistener.cpp
SOURCES += mythuisimpletext.cpp mythuistatetracker.cpp
SOURCES += mythuianimation.cpp mythuiscrollbar.cpp
SOURCES += mythnotificationcenter.cpp mythnotification.cpp
SOURCES += mythuicomposite.cpp
SOURCES += mythedid.cpp
SOURCES += mythhdr.cpp
SOURCES += mythvrr.cpp
SOURCES += mythcolourspace.cpp
SOURCES += devices/mythinputdevicehandler.cpp
SOURCES += mythuiprocedural.cpp
SOURCES += dbsettings.cpp
SOURCES += guistartup.cpp
SOURCES += langsettings.cpp
SOURCES += mediamonitor.cpp
SOURCES += mythterminal.cpp
SOURCES += rawsettingseditor.cpp
SOURCES += schemawizard.cpp
SOURCES += standardsettings.cpp
SOURCES += storagegroupeditor.cpp

using_qtwebengine {
HEADERS += mythuiwebbrowser.h
SOURCES += mythuiwebbrowser.cpp
}

inc.path = $${PREFIX}/include/mythtv/libmythui/

inc.files  = mythrect.h mythmainwindow.h mythpainter.h mythimage.h
inc.files += myththemebase.h themeinfo.h
inc.files += mythuiscreenbounds.h mythuithemecache.h mythuithemehelper.h
inc.files += mythuilocation.h
inc.files += mythpainter_qt.h mythuistatetype.h mythuihelper.h
inc.files += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h
inc.files += mythuitext.h mythuibutton.h mythlistbutton.h xmlparsebase.h
inc.files += myththemedmenu.h mythdialogbox.h mythfontproperties.h
inc.files += mythuiclock.h mythgesture.h mythuitextedit.h mythprogressdialog.h
inc.files += mythuispinbox.h mythuicheckbox.h mythuibuttonlist.h mythuigroup.h
inc.files += mythuiprogressbar.h mythuiwebbrowser.h mythuiutils.h
inc.files += x11colors.h mythgenerictree.h mythuibuttontree.h
inc.files += mythvirtualkeyboard.h mythuishape.h mythuiguidegrid.h
inc.files += mythuieditbar.h mythuifilebrowser.h mythuivideo.h
inc.files += mythuiexp.h mythuisimpletext.h mythuiactions.h
inc.files += mythuistatetracker.h mythuianimation.h mythuiscrollbar.h
inc.files += mythnotificationcenter.h mythnotification.h mythuicomposite.h
inc.files += mythhdr.h mythcolourspace.h
inc.files += langsettings.h
inc.files += mediamonitor.h
inc.files += schemawizard.h
inc.files += standardsettings.h
inc.files += storagegroupeditor.h
inc.files += mythterminal.h

INSTALLS += inc

#
#	Configuration dependent stuff (depending on what is set in mythtv top
#	level settings.pro)
#

using_x11 {
    HEADERS += platforms/mythxdisplay.h
    HEADERS += platforms/mythdisplayx11.h
    HEADERS += platforms/mythscreensaverx11.h
    HEADERS += platforms/mythnvcontrol.h
    SOURCES += platforms/mythxdisplay.cpp
    SOURCES += platforms/mythdisplayx11.cpp
    SOURCES += platforms/mythscreensaverx11.cpp
    SOURCES += platforms/mythnvcontrol.cpp
    freebsd:LIBS += -lXext -lXxf86vm
}

using_drm {
    HEADERS += platforms/mythdisplaydrm.h
    HEADERS += platforms/mythscreensaverdrm.h
    HEADERS += platforms/mythdrmdevice.h
    HEADERS += platforms/drm/mythdrmresources.h
    HEADERS += platforms/drm/mythdrmplane.h
    HEADERS += platforms/drm/mythdrmmode.h
    HEADERS += platforms/drm/mythdrmcrtc.h
    HEADERS += platforms/drm/mythdrmproperty.h
    HEADERS += platforms/drm/mythdrmconnector.h
    HEADERS += platforms/drm/mythdrmencoder.h
    HEADERS += platforms/drm/mythdrmframebuffer.h
    HEADERS += platforms/drm/mythdrmvrr.h
    SOURCES += platforms/mythdisplaydrm.cpp
    SOURCES += platforms/mythscreensaverdrm.cpp
    SOURCES += platforms/mythdrmdevice.cpp
    SOURCES += platforms/drm/mythdrmresources.cpp
    SOURCES += platforms/drm/mythdrmplane.cpp
    SOURCES += platforms/drm/mythdrmmode.cpp
    SOURCES += platforms/drm/mythdrmcrtc.cpp
    SOURCES += platforms/drm/mythdrmproperty.cpp
    SOURCES += platforms/drm/mythdrmconnector.cpp
    SOURCES += platforms/drm/mythdrmencoder.cpp
    SOURCES += platforms/drm/mythdrmframebuffer.cpp
    SOURCES += platforms/drm/mythdrmvrr.cpp
    QMAKE_CXXFLAGS += $${LIBDRM_CFLAGS}

    using_qtprivateheaders {
        HEADERS += platforms/drm/mythdrmhdr.h
        SOURCES += platforms/drm/mythdrmhdr.cpp
    }
}

using_qtprivateheaders {
    QT += gui-private

    using_waylandextras {
        HEADERS += platforms/mythscreensaverwayland.h
        HEADERS += platforms/mythwaylandextras.h
        HEADERS += platforms/waylandprotocols/idle_inhibit_unstable_v1.h
        SOURCES += platforms/mythscreensaverwayland.cpp
        SOURCES += platforms/mythwaylandextras.cpp
        SOURCES += platforms/waylandprotocols/idle_inhibit_unstable_v1.c
        QMAKE_CXXFLAGS += $${LIBWAYLAND_CFLAGS}
    }
}

# Use MMAL as a proxy for Raspberry Pi support
using_mmal {
    HEADERS += platforms/mythdisplayrpi.h
    SOURCES += platforms/mythdisplayrpi.cpp
    LIBS    += -L/opt/vc/lib -lvchostif -lvchiq_arm
    QMAKE_CXXFLAGS += -isystem /opt/vc/include
}

using_qtdbus {
    QT      += dbus
    HEADERS += platforms/mythscreensaverdbus.h
    SOURCES += platforms/mythscreensaverdbus.cpp
    HEADERS += platforms/mythdisplaymutter.h
    SOURCES += platforms/mythdisplaymutter.cpp
}

unix:!cygwin {
    SOURCES += mediamonitor-unix.cpp
    HEADERS += mediamonitor-unix.h
    !android {
        using_qtdbus: QT += dbus
    }
}

mingw | win32-msvc* {
    SOURCES += mediamonitor-windows.cpp
    HEADERS += mediamonitor-windows.h
}

macx {
    HEADERS += platforms/mythscreensaverosx.h
    HEADERS += platforms/mythosxutils.h
    SOURCES += platforms/mythscreensaverosx.cpp
    SOURCES += platforms/mythosxutils.cpp
    HEADERS += platforms/mythdisplayosx.h
    SOURCES += platforms/mythdisplayosx.cpp
    QMAKE_OBJECTIVE_CFLAGS += $$QMAKE_CXXFLAGS
    QMAKE_OBJECTIVE_CXXFLAGS += $$QMAKE_CXXFLAGS
    OBJECTIVE_HEADERS += platforms/mythutilscocoa.h
    OBJECTIVE_SOURCES += platforms/mythutilscocoa.mm
    LIBS              += -framework Cocoa -framework IOKit

    using_appleremote {
        HEADERS += devices/AppleRemote.h   devices/AppleRemoteListener.h
        SOURCES += devices/AppleRemote.cpp devices/AppleRemoteListener.cpp
        !using_lirc: HEADERS += devices/lircevent.h
        !using_lirc: SOURCES += devices/lircevent.cpp
    }

    darwin_da {
        SOURCES -= mediamonitor-unix.cpp
        HEADERS -= mediamonitor-unix.h
        HEADERS += mediamonitor-darwin.h
        SOURCES += mediamonitor-darwin.cpp
        LIBS += -framework DiskArbitration
    }
}

android {
    HEADERS += platforms/mythscreensaverandroid.h
    HEADERS += platforms/mythdisplayandroid.h
    SOURCES += platforms/mythscreensaverandroid.cpp
    SOURCES += platforms/mythdisplayandroid.cpp
}

using_joystick_menu {
    HEADERS += devices/jsmenu.h devices/jsmenuevent.h
    SOURCES += devices/jsmenu.cpp devices/jsmenuevent.cpp
}

using_lirc {
    HEADERS += devices/lirc.h   devices/lircevent.h   devices/lirc_client.h
    SOURCES += devices/lirc.cpp devices/lircevent.cpp devices/lirc_client.cpp
}

using_libcec {
    HEADERS += devices/mythcecadapter.h
    SOURCES += devices/mythcecadapter.cpp
}

cygwin:DEFINES += _WIN32

mingw | win32-msvc*{
#   HEADERS += mythpainter_d3d9.h   mythrender_d3d9.h
#   SOURCES += mythpainter_d3d9.cpp mythrender_d3d9.cpp
    HEADERS += platforms/mythdisplaywindows.h
    SOURCES += platforms/mythdisplaywindows.cpp
    DEFINES += NODRAWTEXT
    LIBS    += -luser32  -lgdi32
}

using_vulkan {
    HEADERS += vulkan/mythpainterwindowvulkan.h
    HEADERS += vulkan/mythpaintervulkan.h
    HEADERS += vulkan/mythrendervulkan.h
    HEADERS += vulkan/mythwindowvulkan.h
    HEADERS += vulkan/mythtexturevulkan.h
    HEADERS += vulkan/mythshadervulkan.h
    HEADERS += vulkan/mythshadersvulkan.h
    HEADERS += vulkan/mythuniformbuffervulkan.h
    HEADERS += vulkan/mythcombobuffervulkan.h
    HEADERS += vulkan/mythdebugvulkan.h
    HEADERS += vulkan/mythvertexbuffervulkan.h
    SOURCES += vulkan/mythpainterwindowvulkan.cpp
    SOURCES += vulkan/mythpaintervulkan.cpp
    SOURCES += vulkan/mythrendervulkan.cpp
    SOURCES += vulkan/mythwindowvulkan.cpp
    SOURCES += vulkan/mythtexturevulkan.cpp
    SOURCES += vulkan/mythshadervulkan.cpp
    SOURCES += vulkan/mythuniformbuffervulkan.cpp
    SOURCES += vulkan/mythcombobuffervulkan.cpp
    SOURCES += vulkan/mythdebugvulkan.cpp
    SOURCES += vulkan/mythvertexbuffervulkan.cpp
}

using_opengl {
    HEADERS += opengl/mythpainterwindowopengl.h
    HEADERS += opengl/mythpainteropengl.h
    HEADERS += opengl/mythrenderopengl.h
    HEADERS += opengl/mythrenderopengldefs.h
    HEADERS += opengl/mythrenderopenglshaders.h
    HEADERS += opengl/mythopenglperf.h
    HEADERS += opengl/mythegl.h
    SOURCES += opengl/mythpainterwindowopengl.cpp
    SOURCES += opengl/mythpainteropengl.cpp
    SOURCES += opengl/mythrenderopengl.cpp
    SOURCES += opengl/mythopenglperf.cpp
    SOURCES += opengl/mythegl.cpp

    using_egl {
        LIBS    += -lEGL
    }

    mingw|win32-msvc*:LIBS += -lopengl32
}

DEFINES += MUI_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
