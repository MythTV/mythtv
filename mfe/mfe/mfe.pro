

include ( ../settings.pro )

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythmfe
target.path = $${PREFIX}/lib/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = mfe-ui.xml images/*.png


INSTALLS += uifiles



HEADERS +=          mfedialog.h   mfdinfo.h   playlistdialog.h   netflasher.h
HEADERS += visualize/visual.h   visualize/visualnode.h   visualize/stereoscope.h
HEADERS += visualize/inlines.h  visualize/visualwrapper.h
HEADERS += visualize/monoscope.h

SOURCES += main.cpp mfedialog.cpp mfdinfo.cpp playlistdialog.cpp netflasher.cpp
SOURCES += visualize/visual.cpp visualize/visualnode.cpp visualize/stereoscope.cpp
SOURCES += visualize/visualwrapper.cpp
SOURCES += visualize/monoscope.cpp

LIBS += -L$${PREFIX}/lib -lmfdclient
