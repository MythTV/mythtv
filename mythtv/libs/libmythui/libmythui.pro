include ( ../../settings.pro )

QT += xml sql network opengl
contains(QT_VERSION, ^4\\.[0-9]\\..*) {
QT += webkit
}
contains(QT_VERSION, ^5\\.[0-9]\\..*):using_qtwebkit {
QT += widgets
QT += webkitwidgets
android: QT += androidextras
}

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH += ../libmythbase
INCLUDEPATH += ../.. ../
INCLUDEPATH += ../../external/FFmpeg

LIBS += -L../libmythbase -lmythbase-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS  = mythmainwindow.h mythpainter.h mythimage.h mythrect.h
HEADERS += myththemebase.h  mythpainter_qimage.h mythpainter_yuva.h
HEADERS += mythpainter_qt.h mythmainwindow_internal.h mythuihelper.h
HEADERS += mythscreenstack.h mythgesture.h mythuitype.h mythscreentype.h
HEADERS += mythuiimage.h mythuitext.h mythuistatetype.h  xmlparsebase.h
HEADERS += mythuibutton.h myththemedmenu.h mythdialogbox.h
HEADERS += mythuiclock.h mythuitextedit.h mythprogressdialog.h mythuispinbox.h
HEADERS += mythuicheckbox.h mythuibuttonlist.h mythuigroup.h
HEADERS += mythuiprogressbar.h mythuifilebrowser.h
HEADERS += screensaver.h screensaver-null.h x11colors.h
HEADERS += themeinfo.h mythxdisplay.h DisplayRes.h DisplayResScreen.h
HEADERS += mythgenerictree.h mythuibuttontree.h mythuiutils.h
HEADERS += mythvirtualkeyboard.h mythuishape.h mythuiguidegrid.h
HEADERS += mythrender_base.h mythfontmanager.h mythuieditbar.h
HEADERS += mythdisplay.h mythuivideo.h mythudplistener.h
HEADERS += mythuiexp.h mythuisimpletext.h mythuistatetracker.h
HEADERS += mythuianimation.h mythuiscrollbar.h
HEADERS += mythnotificationcenter.h mythnotificationcenter_private.h
HEADERS += mythuicomposite.h mythnotification.h mythuidefines.h

SOURCES  = mythmainwindow.cpp mythpainter.cpp mythimage.cpp mythrect.cpp
SOURCES += myththemebase.cpp  mythpainter_qimage.cpp mythpainter_yuva.cpp
SOURCES += mythpainter_qt.cpp xmlparsebase.cpp mythuihelper.cpp
SOURCES += mythscreenstack.cpp mythgesture.cpp mythuitype.cpp mythscreentype.cpp
SOURCES += mythuiimage.cpp mythuitext.cpp mythuifilebrowser.cpp
SOURCES += mythuistatetype.cpp mythfontproperties.cpp
SOURCES += mythuibutton.cpp myththemedmenu.cpp mythdialogbox.cpp
SOURCES += mythuiclock.cpp mythuitextedit.cpp mythprogressdialog.cpp
SOURCES += mythuispinbox.cpp mythuicheckbox.cpp mythuibuttonlist.cpp
SOURCES += mythuigroup.cpp mythuiprogressbar.cpp
SOURCES += screensaver.cpp screensaver-null.cpp x11colors.cpp
SOURCES += themeinfo.cpp mythxdisplay.cpp DisplayRes.cpp DisplayResScreen.cpp
SOURCES += mythgenerictree.cpp mythuibuttontree.cpp mythuiutils.cpp
SOURCES += mythvirtualkeyboard.cpp mythuishape.cpp mythuiguidegrid.cpp
SOURCES += mythfontmanager.cpp mythuieditbar.cpp
SOURCES += mythdisplay.cpp mythuivideo.cpp mythudplistener.cpp
SOURCES += mythuisimpletext.cpp mythuistatetracker.cpp
SOURCES += mythuianimation.cpp mythuiscrollbar.cpp
SOURCES += mythnotificationcenter.cpp mythnotification.cpp
SOURCES += mythuicomposite.cpp

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

using_vdpau {
    DEFINES += USING_VDPAU
    HEADERS += mythpainter_vdpau.h   mythrender_vdpau.h
    SOURCES += mythpainter_vdpau.cpp mythrender_vdpau.cpp
    LIBS += -lvdpau
}

using_x11 {
    DEFINES += USING_X11
    HEADERS += screensaver-x11.h
    SOURCES += screensaver-x11.cpp
    # Add nvidia XV-EXTENSION support
    HEADERS += util-nvctrl.h
    SOURCES += util-nvctrl.cpp
    INCLUDEPATH += ../../external/libXNVCtrl
    LIBS += -L../../external/libXNVCtrl -lmythXNVCtrl-$${LIBVERSION}
    POST_TARGETDEPS += ../../external/libXNVCtrl/libmythXNVCtrl-$${LIBVERSION}.$${QMAKE_EXTENSION_STATICLIB}
}

using_qtdbus {
    QT      += dbus
    DEFINES += USING_DBUS
    HEADERS += screensaver-dbus.h
    SOURCES += screensaver-dbus.cpp
}

macx {
    HEADERS += screensaver-osx.h   DisplayResOSX.h   util-osx.h
    SOURCES += screensaver-osx.cpp DisplayResOSX.cpp util-osx.cpp
    QMAKE_OBJECTIVE_CFLAGS += $$QMAKE_CXXFLAGS
    QMAKE_OBJECTIVE_CXXFLAGS += $$QMAKE_CXXFLAGS
    OBJECTIVE_HEADERS += util-osx-cocoa.h
    OBJECTIVE_SOURCES += util-osx-cocoa.mm
    LIBS              += -framework Cocoa -framework IOKit

    using_appleremote {
        HEADERS += AppleRemote.h   AppleRemoteListener.h
        SOURCES += AppleRemote.cpp AppleRemoteListener.cpp
        !using_lirc: HEADERS += lircevent.h
        !using_lirc: SOURCES += lircevent.cpp
    }
}

android {
    HEADERS += screensaver-android.h
    SOURCES += screensaver-android.cpp
}

using_joystick_menu {
    DEFINES += USE_JOYSTICK_MENU
    HEADERS += jsmenu.h jsmenuevent.h
    SOURCES += jsmenu.cpp jsmenuevent.cpp
}

using_lirc {
    DEFINES += USE_LIRC
    HEADERS += lirc.h   lircevent.h   lirc_client.h
    SOURCES += lirc.cpp lircevent.cpp lirc_client.c
}

using_libcec {
    DEFINES += USING_LIBCEC
    HEADERS += cecadapter.h
    SOURCES += cecadapter.cpp
}

using_xrandr {
    DEFINES += USING_XRANDR
    HEADERS += DisplayResX.h
    SOURCES += DisplayResX.cpp
}

cygwin:DEFINES += _WIN32
mingw :DEFINES += USING_MINGW

mingw | win32-msvc*{
    HEADERS += mythpainter_d3d9.h   mythrender_d3d9.h
    SOURCES += mythpainter_d3d9.cpp mythrender_d3d9.cpp
    DEFINES += NODRAWTEXT
    LIBS    += -lGdi32 -lUser32

    using_dxva2: DEFINES += USING_DXVA2
}

using_opengl {
    using_opengl_themepainter:DEFINES += USE_OPENGL_PAINTER
    SOURCES += mythpainter_ogl.cpp  mythrender_opengl.cpp
    SOURCES += mythrender_opengl2.cpp
    HEADERS += mythpainter_ogl.h    mythrender_opengl.h mythrender_opengl_defs.h
    HEADERS += mythrender_opengl2.h mythrender_opengl_defs2.h
    using_opengles {
        DEFINES += USING_OPENGLES
        HEADERS += mythrender_opengl2es.h
    }
    !using_opengles {
        SOURCES += mythrender_opengl1.cpp
        HEADERS += mythrender_opengl1.h mythrender_opengl_defs1.h
    }
    inc.files += mythpainter_ogl.h
    QT += opengl

    mingw|win32-msvc*:LIBS += -lopengl32
}

using_openmax {
    contains( HAVE_OPENMAX_BROADCOM, yes ) {
        using_opengl {
            # For raspberry Pi Raspbian
            exists(/opt/vc/lib/libbrcmEGL.so) {
                LIBS += -L/opt/vc/lib/ -lbrcmGLESv2 -lbrcmEGL
            }
        }
    }
}


DEFINES += USING_QTWEBKIT
DEFINES += MUI_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
