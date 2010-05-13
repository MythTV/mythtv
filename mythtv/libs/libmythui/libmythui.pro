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
HEADERS += mythscreenstack.h mythgesture.h mythuitype.h mythscreentype.h
HEADERS += mythuiimage.h mythuitext.h mythuistatetype.h  xmlparsebase.h
HEADERS += mythuibutton.h myththemedmenu.h mythdialogbox.h
HEADERS += mythuiclock.h mythuitextedit.h mythprogressdialog.h mythuispinbox.h
HEADERS += mythuicheckbox.h mythuibuttonlist.h mythuigroup.h
HEADERS += mythuiprogressbar.h mythuiwebbrowser.h
HEADERS += screensaver.h screensaver-null.h x11colors.h
HEADERS += themeinfo.h mythxdisplay.h DisplayRes.h DisplayResScreen.h
HEADERS += mythgenerictree.h mythuibuttontree.h mythuiutils.h
HEADERS += mythvirtualkeyboard.h mythuishape.h mythuiguidegrid.h
HEADERS += mythrender_base.h mythfontmanager.h

SOURCES  = mythmainwindow.cpp mythpainter.cpp mythimage.cpp mythrect.cpp
SOURCES += myththemebase.cpp
SOURCES += mythpainter_qt.cpp xmlparsebase.cpp mythuihelper.cpp
SOURCES += mythscreenstack.cpp mythgesture.cpp mythuitype.cpp mythscreentype.cpp 
SOURCES +=  mythuiimage.cpp mythuitext.cpp
SOURCES += mythuistatetype.cpp mythfontproperties.cpp
SOURCES += mythuibutton.cpp myththemedmenu.cpp mythdialogbox.cpp
SOURCES += mythuiclock.cpp mythuitextedit.cpp mythprogressdialog.cpp
SOURCES += mythuispinbox.cpp mythuicheckbox.cpp mythuibuttonlist.cpp
SOURCES += mythuigroup.cpp mythuiprogressbar.cpp mythuiwebbrowser.cpp
SOURCES += screensaver.cpp screensaver-null.cpp x11colors.cpp
SOURCES += themeinfo.cpp mythxdisplay.cpp DisplayRes.cpp DisplayResScreen.cpp
SOURCES += mythgenerictree.cpp mythuibuttontree.cpp mythuiutils.cpp
SOURCES += mythvirtualkeyboard.cpp mythuishape.cpp mythuiguidegrid.cpp
SOURCES += mythfontmanager.cpp

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
inc.files += x11colors.h mythgenerictree.h mythuibuttontree.h
inc.files += mythvirtualkeyboard.h mythuishape.h mythuiguidegrid.h

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
    HEADERS += screensaver-x11.h screensaver-xdg.h
    SOURCES += screensaver-x11.cpp screensaver-xdg.cpp
    LIBS += $$EXTRA_LIBS
}

macx {
    HEADERS += screensaver-osx.h   DisplayResOSX.h   util-osx.h
    SOURCES += screensaver-osx.cpp DisplayResOSX.cpp util-osx.cpp

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/ApplicationServices.framework/Frameworks
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/Carbon.framework/Frameworks
    LIBS           += -framework Carbon -framework IOKit

    using_appleremote {
        HEADERS += AppleRemote.h   AppleRemoteListener.h
        SOURCES += AppleRemote.cpp AppleRemoteListener.cpp
        !using_lirc: HEADERS += lircevent.h
        !using_lirc: SOURCES += lircevent.cpp
    }
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

using_xrandr {
    DEFINES += USING_XRANDR
    HEADERS += DisplayResX.h
    SOURCES += DisplayResX.cpp
    # Add nvidia XV-EXTENSION support
    SOURCES += util-nvctrl.cpp
    LIBS += -L../libmythnvctrl -lmythnvctrl-$${LIBVERSION}
    TARGETDEPS += ../libmythnvctrl/libmythnvctrl-$${MYTH_LIB_EXT}
}

cygwin:DEFINES += _WIN32

using_opengl {
    DEFINES += USE_OPENGL_PAINTER
    SOURCES += mythpainter_ogl.cpp
    HEADERS += mythpainter_ogl.h
    inc.files += mythpainter_ogl.h
    QT += opengl

    using_x11:LIBS += $$EXTRA_LIBS
    mingw:LIBS += -lopengl32
}

QT += xml sql network webkit
DEFINES += USING_QTWEBKIT

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
