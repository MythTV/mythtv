include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythui-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

# Input
HEADERS  = mythmainwindow.h mythpainter.h mythimage.h
HEADERS += mythpainter_ogl.h mythpainter_qt.h
HEADERS += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
HEADERS += mythuitext.h mythuianimatedimage.h mythlistbutton.h
HEADERS += mythcontext.h oldsettings.h remotefile.h util.h themedmenu.h
HEADERS += dialogbox.h

SOURCES  = mythmainwindow.cpp mythpainter.cpp mythimage.cpp
SOURCES += mythpainter_ogl.cpp mythpainter_qt.cpp
SOURCES += mythscreenstack.cpp mythscreentype.cpp 
SOURCES += mythuitype.cpp mythuiimage.cpp mythuitext.cpp mythuianimatedimage.cpp
SOURCES += mythlistbutton.cpp
SOURCES += mythcontext.cpp oldsettings.cpp remotefile.cpp themedmenu.cpp 
SOURCES += util.cpp
SOURCES += dialogbox.cpp

inc.path = $${PREFIX}/include/mythui/

inc.files  = mythmainwindow.h mythpainter.h mythimage.h
inc.files += mythpainter_ogl.h mythpainter_qt.h
inc.files += mythscreenstack.h mythscreentype.h mythuitype.h mythuiimage.h 
inc.files += mythuitext.h mythuianimatedimage.h mythlistbutton.h
inc.files += mythcontext.h oldsettings.h remotefile.h util.h themedmenu.h
inc.files += dialogbox.h

INSTALLS += inc
