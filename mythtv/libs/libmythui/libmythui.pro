include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH += ../libmythdb
INCLUDEPATH += ../.. ../

LIBS += -L../libmythdb -lmythdb-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS  = mythmainwindow.h mythpainter.h mythimage.h mythrect.h
HEADERS += myththemebase.h
HEADERS += mythpainter_qt.h mythmainwindow_internal.h mythuihelper.h
HEADERS += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h
HEADERS += mythuitext.h mythuistatetype.h mythgesture.h xmlparsebase.h
HEADERS += mythuibutton.h myththemedmenu.h mythdialogbox.h
HEADERS += mythuiclock.h mythuitextedit.h mythprogressdialog.h mythuispinbox.h
HEADERS += mythuicheckbox.h mythuibuttonlist.h mythuigroup.h
HEADERS += mythuiprogressbar.h mythuiwebbrowser.h
HEADERS += screensaver.h screensaver-null.h mythsystem.h x11colors.h
HEADERS += themeinfo.h util-x11.h DisplayRes.h DisplayResScreen.h
HEADERS += mythgenerictree.h mythuibuttontree.h mythuiutils.h

SOURCES  = mythmainwindow.cpp mythpainter.cpp mythimage.cpp mythrect.cpp
SOURCES += myththemebase.cpp
SOURCES += mythpainter_qt.cpp xmlparsebase.cpp mythuihelper.cpp
SOURCES += mythscreenstack.cpp mythscreentype.cpp mythgesture.cpp
SOURCES += mythuitype.cpp mythuiimage.cpp mythuitext.cpp
SOURCES += mythuistatetype.cpp mythfontproperties.cpp
SOURCES += mythuibutton.cpp myththemedmenu.cpp mythdialogbox.cpp
SOURCES += mythuiclock.cpp mythuitextedit.cpp mythprogressdialog.cpp
SOURCES += mythuispinbox.cpp mythuicheckbox.cpp mythuibuttonlist.cpp
SOURCES += mythuigroup.cpp mythuiprogressbar.cpp mythuiwebbrowser.cpp
SOURCES += screensaver.cpp screensaver-null.cpp mythsystem.cpp x11colors.cpp
SOURCES += themeinfo.cpp util-x11.cpp DisplayRes.cpp DisplayResScreen.cpp
SOURCES += mythgenerictree.cpp mythuibuttontree.cpp mythuiutils.cpp

inc.path = $${PREFIX}/include/mythtv/libmythui/

inc.files  = mythrect.h mythmainwindow.h mythpainter.h mythimage.h
inc.files += myththemebase.h
inc.files += mythpainter_qt.h mythuistatetype.h mythuihelper.h
inc.files += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h
inc.files += mythuitext.h mythuibutton.h mythlistbutton.h xmlparsebase.h
inc.files += myththemedmenu.h mythdialogbox.h mythfontproperties.h
inc.files += mythuiclock.h mythgesture.h mythuitextedit.h mythprogressdialog.h
inc.files += mythuispinbox.h mythuicheckbox.h mythuibuttonlist.h mythuigroup.h
inc.files += mythuiprogressbar.h mythuiwebbrowser.h mythuiutils.h
inc.files += mythsystem.h x11colors.h mythgenerictree.h mythuibuttontree.h

INSTALLS += inc

#
#	Configuration dependent stuff (depending on what is set in mythtv top
#	level settings.pro)
#

using_x11:using_opengl {
    DEFINES += USE_OPENGL_PAINTER
    SOURCES += mythpainter_ogl.cpp
    HEADERS += mythpainter_ogl.h
    inc.files += mythpainter_ogl.h
    LIBS += $$EXTRA_LIBS
}

using_vdpau {
    DEFINES += USING_VDPAU
    HEADERS += mythpainter_vdpau.h
    SOURCES += mythpainter_vdpau.cpp
    LIBS += -lvdpau
}

using_x11 {
    DEFINES += USING_X11
    HEADERS += screensaver-x11.h
    SOURCES += screensaver-x11.cpp
    LIBS += $$EXTRA_LIBS
}

macx {
    HEADERS += screensaver-osx.h   DisplayResOSX.h   util-osx.h
    SOURCES += screensaver-osx.cpp DisplayResOSX.cpp util-osx.cpp

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/ApplicationServices.framework/Frameworks
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/Carbon.framework/Frameworks
    LIBS           += -framework Carbon -framework OpenGL

    using_appleremote {
        HEADERS += AppleRemote.h   AppleRemoteListener.h
        SOURCES += AppleRemote.cpp AppleRemoteListener.cpp
        !using_lirc: HEADERS += lircevent.h
        !using_lirc: SOURCES += lircevent.cpp
    }

    QMAKE_LFLAGS_SHLIB += -flat_namespace
}

using_joystick_menu {
    DEFINES += USE_JOYSTICK_MENU
    HEADERS += jsmenu.h jsmenuevent.h
    SOURCES += jsmenu.cpp jsmenuevent.cpp
}

using_lirc {
    DEFINES += USE_LIRC
    HEADERS += lirc.h lircevent.h
    SOURCES += lirc.cpp lircevent.cpp
    LIBS += $$LIRC_LIBS
}

using_xrandr {
    DEFINES += USING_XRANDR
    HEADERS += DisplayResX.h
    SOURCES += DisplayResX.cpp
}

cygwin:DEFINES += _WIN32

mingw {
    using_opengl {
        LIBS += -lopengl32
        DEFINES += USE_OPENGL_PAINTER
        SOURCES += mythpainter_ogl.cpp
        HEADERS += mythpainter_ogl.h
        inc.files += mythpainter_ogl.h
    }
}

#The following line was inserted by qt3to4
QT += xml sql opengl network

using_qtwebkit {
        QT += webkit
        DEFINES += USING_QTWEBKIT
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
