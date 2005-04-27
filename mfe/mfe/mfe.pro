

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

SOURCES += main.cpp mfedialog.cpp mfdinfo.cpp playlistdialog.cpp netflasher.cpp

LIBS += -L$${PREFIX}/lib -lmfdclient
