include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

html.path = $${PREFIX}/share/mythtv/html/
html.files = frontend_index.qsp backend_index.qsp overview.html menu.qsp robots.txt
html.files += css images js misc setup samples tv
html.files += 3rdParty xslt video debug

INSTALLS += html

# Input
SOURCES += dummy.c

OTHER_FILES += \
    video/gallery.qsp
