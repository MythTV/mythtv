include (../../mythconfig.mak )
include (../../settings.pro )

INCLUDEPATH += /usr/include/kde
INCLUDEPATH += /opt/kde3/include

TEMPLATE = app
CONFIG += thread opengl
TARGET = mythbrowser
target.path = $${PREFIX}/bin
INSTALLS += target

installimages.path = $${PREFIX}/share/mythtv/themes/default
installimages.files = images/*.png

INSTALLS += installimages

LIBS += -lmyth-$$LIBVERSION -lmythui-$$LIBVERSION $$EXTRA_LIBS
LIBS += -L/opt/kde3/lib -lkhtml

# Input
HEADERS += webpage.h tabview.h
SOURCES += main.cpp webpage.cpp tabview.cpp
 
