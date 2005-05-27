

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


HEADERS += visualize/goom/filters.h visualize/goom/goomconfig.h visualize/goom/goom_core.h visualize/goom/graphic.h
HEADERS += visualize/goom/ifs.h visualize/goom/lines.h visualize/goom/mythgoom.h visualize/goom/drawmethods.h
HEADERS += visualize/goom/mmx.h visualize/goom/mathtools.h visualize/goom/tentacle3d.h visualize/goom/v3d.h

SOURCES += visualize/goom/filters.c visualize/goom/goom_core.c visualize/goom/graphic.c visualize/goom/tentacle3d.c
SOURCES += visualize/goom/ifs.c visualize/goom/ifs_display.c visualize/goom/lines.c visualize/goom/surf3d.c 
SOURCES += visualize/goom/zoom_filter_mmx.c visualize/goom/zoom_filter_xmmx.c visualize/goom/mythgoom.cpp

DEFINES += HAVE_MMX

LIBS += -L$${PREFIX}/lib -lmfdclient
