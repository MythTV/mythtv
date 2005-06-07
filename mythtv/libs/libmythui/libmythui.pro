include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += debug thread dll 
target.path = $${PREFIX}/lib
INSTALLS = target
INCLUDEPATH += ../../libs/libmyth
INCLUDEPATH += ../..
LIBS += -L../libmyth -lmyth-$$LIBVERSION
VERSION = 0.18.0

# Input
HEADERS  = mythmainwindow.h mythpainter.h mythimage.h
HEADERS += mythpainter_ogl.h mythpainter_qt.h
HEADERS += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
HEADERS += mythuitext.h mythuistateimage.h
HEADERS += mythuibutton.h mythlistbutton.h themedmenu.h dialogbox.h

SOURCES  = mythmainwindow.cpp mythpainter.cpp mythimage.cpp
SOURCES += mythpainter_ogl.cpp mythpainter_qt.cpp
SOURCES += mythscreenstack.cpp mythscreentype.cpp 
SOURCES += mythuitype.cpp mythuiimage.cpp mythuitext.cpp
SOURCES += mythuistateimage.cpp mythlistbutton.cpp mythfontproperties.cpp
SOURCES += mythuibutton.cpp themedmenu.cpp dialogbox.cpp

inc.path = $${PREFIX}/include/mythui/

inc.files  = mythmainwindow.h mythpainter.h mythimage.h
inc.files += mythpainter_ogl.h mythpainter_qt.h
inc.files += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
inc.files += mythuitext.h mythuibutton.h mythlistbutton.h
inc.files += themedmenu.h dialogbox.h mythfontproperties.h

INSTALLS += inc

#
#	Configuration dependent stuff (depending on what is set in mythtv top
#	level settings.pro)
#

using_x11 {
    LIBS += $$EXTRA_LIBS
}

macx {
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/Carbon.framework/Frameworks
    LIBS           += -framework Carbon -framework OpenGL

    # Duplication of source with libmyth (e.g. oldsettings.cpp)
    # means that the linker complains, so we have to ignore duplicates 
    QMAKE_LFLAGS_SHLIB += -multiply_defined suppress

    QMAKE_LFLAGS_SHLIB += -seg1addr 0xCC000000
}

