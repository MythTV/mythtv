include ( ../../settings.pro )

QT += xml sql network widgets
using_qtwebkit {
    QT += webkitwidgets
}
android: QT += androidextras

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

DEPENDPATH  += ./opengl ./platforms ./devices ./vulkan
INCLUDEPATH += $$DEPENDPATH
INCLUDEPATH += ../libmythbase
INCLUDEPATH += ../.. ../

LIBS += -L../libmythbase -lmythbase-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS  = mythmainwindowprivate.h mythmainwindow.h mythpainter.h mythimage.h mythrect.h
HEADERS += mythpainterwindow.h mythpainterwindowqt.h
HEADERS += myththemebase.h
HEADERS += mythpainter_qt.h mythuihelper.h
HEADERS += mythscreenstack.h mythgesture.h mythuitype.h mythscreentype.h
HEADERS += mythuiimage.h mythuitext.h mythuistatetype.h  xmlparsebase.h
HEADERS += mythuibutton.h myththemedmenu.h mythdialogbox.h
HEADERS += mythuiclock.h mythuitextedit.h mythprogressdialog.h mythuispinbox.h
HEADERS += mythuicheckbox.h mythuibuttonlist.h mythuigroup.h
HEADERS += mythuiprogressbar.h mythuifilebrowser.h
HEADERS += screensaver.h screensaver-null.h x11colors.h
HEADERS += themeinfo.h platforms/mythxdisplay.h mythdisplaymode.h
HEADERS += mythgenerictree.h mythuibuttontree.h mythuiutils.h
HEADERS += mythvirtualkeyboard.h mythuishape.h mythuiguidegrid.h
HEADERS += mythrender_base.h mythfontmanager.h mythuieditbar.h
HEADERS += mythdisplay.h mythuivideo.h mythudplistener.h
HEADERS += mythuiexp.h mythuisimpletext.h mythuistatetracker.h
HEADERS += mythuianimation.h mythuiscrollbar.h
HEADERS += mythnotificationcenter.h mythnotificationcenter_private.h
HEADERS += mythuicomposite.h mythnotification.h
HEADERS += mythedid.h
HEADERS += devices/mythinputdevicehandler.h

SOURCES  = mythmainwindowprivate.cpp mythmainwindow.cpp mythpainter.cpp mythimage.cpp mythrect.cpp
SOURCES += mythpainterwindow.cpp mythpainterwindowqt.cpp
SOURCES += myththemebase.cpp
SOURCES += mythpainter_qt.cpp xmlparsebase.cpp mythuihelper.cpp
SOURCES += mythscreenstack.cpp mythgesture.cpp mythuitype.cpp mythscreentype.cpp
SOURCES += mythuiimage.cpp mythuitext.cpp mythuifilebrowser.cpp
SOURCES += mythuistatetype.cpp mythfontproperties.cpp
SOURCES += mythuibutton.cpp myththemedmenu.cpp mythdialogbox.cpp
SOURCES += mythuiclock.cpp mythuitextedit.cpp mythprogressdialog.cpp
SOURCES += mythuispinbox.cpp mythuicheckbox.cpp mythuibuttonlist.cpp
SOURCES += mythuigroup.cpp mythuiprogressbar.cpp
SOURCES += screensaver.cpp screensaver-null.cpp x11colors.cpp
SOURCES += themeinfo.cpp platforms/mythxdisplay.cpp mythdisplaymode.cpp
SOURCES += mythgenerictree.cpp mythuibuttontree.cpp mythuiutils.cpp
SOURCES += mythvirtualkeyboard.cpp mythuishape.cpp mythuiguidegrid.cpp
SOURCES += mythfontmanager.cpp mythuieditbar.cpp
SOURCES += mythdisplay.cpp mythuivideo.cpp mythudplistener.cpp
SOURCES += mythuisimpletext.cpp mythuistatetracker.cpp
SOURCES += mythuianimation.cpp mythuiscrollbar.cpp
SOURCES += mythnotificationcenter.cpp mythnotification.cpp
SOURCES += mythuicomposite.cpp
SOURCES += mythedid.cpp
SOURCES += devices/mythinputdevicehandler.cpp

using_qtwebkit {
HEADERS += mythuiwebbrowser.h
SOURCES += mythuiwebbrowser.cpp
}

inc.path = $${PREFIX}/include/mythtv/libmythui/

inc.files  = mythrect.h mythmainwindow.h mythpainter.h mythimage.h
inc.files += myththemebase.h themeinfo.h
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

INSTALLS += inc

#
#	Configuration dependent stuff (depending on what is set in mythtv top
#	level settings.pro)
#

using_x11 {
    DEFINES += USING_X11
    HEADERS += screensaver-x11.h
    SOURCES += screensaver-x11.cpp
    HEADERS += platforms/mythdisplayx11.h
    SOURCES += platforms/mythdisplayx11.cpp
}

using_drm {
    DEFINES += USING_DRM
    HEADERS += platforms/mythdisplaydrm.h
    HEADERS += platforms/mythdrmdevice.h
    SOURCES += platforms/mythdisplaydrm.cpp
    SOURCES += platforms/mythdrmdevice.cpp
    QMAKE_CXXFLAGS += $${LIBDRM_CFLAGS}
}

# Use MMAL as a proxy for Raspberry Pi support
using_mmal {
    DEFINES += USING_MMAL
    HEADERS += platforms/mythdisplayrpi.h
    SOURCES += platforms/mythdisplayrpi.cpp
    LIBS    += -L/opt/vc/lib -lvchostif -lvchiq_arm
    QMAKE_CXXFLAGS += -isystem /opt/vc/include
}

using_qtdbus {
    QT      += dbus
    DEFINES += USING_DBUS
    HEADERS += screensaver-dbus.h
    SOURCES += screensaver-dbus.cpp
}

macx {
    HEADERS += screensaver-osx.h   platforms/mythosxutils.h
    SOURCES += screensaver-osx.cpp platforms/mythosxutils.cpp
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
}

android {
    HEADERS += screensaver-android.h   platforms/mythdisplayandroid.h
    SOURCES += screensaver-android.cpp platforms/mythdisplayandroid.cpp
}

using_joystick_menu {
    DEFINES += USE_JOYSTICK_MENU
    HEADERS += devices/jsmenu.h devices/jsmenuevent.h
    SOURCES += devices/jsmenu.cpp devices/jsmenuevent.cpp
}

using_lirc {
    DEFINES += USE_LIRC
    HEADERS += devices/lirc.h   devices/lircevent.h   devices/lirc_client.h
    SOURCES += devices/lirc.cpp devices/lircevent.cpp devices/lirc_client.cpp
}

using_libcec {
    DEFINES += USING_LIBCEC
    HEADERS += devices/mythcecadapter.h
    SOURCES += devices/mythcecadapter.cpp
}

using_xrandr {
    DEFINES += USING_XRANDR
}

cygwin:DEFINES += _WIN32
mingw :DEFINES += USING_MINGW

mingw | win32-msvc*{
    HEADERS += mythpainter_d3d9.h   mythrender_d3d9.h
    SOURCES += mythpainter_d3d9.cpp mythrender_d3d9.cpp
    HEADERS += platforms/mythdisplaywindows.h
    SOURCES += platforms/mythdisplaywindows.cpp
    DEFINES += NODRAWTEXT
    LIBS    += -lGdi32 -lUser32

    using_dxva2: DEFINES += USING_DXVA2
}

using_vulkan {
    DEFINES += USING_VULKAN
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
    SOURCES += vulkan/mythpainterwindowvulkan.cpp
    SOURCES += vulkan/mythpaintervulkan.cpp
    SOURCES += vulkan/mythrendervulkan.cpp
    SOURCES += vulkan/mythwindowvulkan.cpp
    SOURCES += vulkan/mythtexturevulkan.cpp
    SOURCES += vulkan/mythshadervulkan.cpp
    SOURCES += vulkan/mythuniformbuffervulkan.cpp
    SOURCES += vulkan/mythcombobuffervulkan.cpp
    SOURCES += vulkan/mythdebugvulkan.cpp
    using_libglslang: DEFINES += USING_GLSLANG
}

using_opengl {
    DEFINES += USING_OPENGL
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
        DEFINES += USING_EGL
    }

    mingw|win32-msvc*:LIBS += -lopengl32
}

DEFINES += USING_QTWEBKIT
DEFINES += MUI_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
