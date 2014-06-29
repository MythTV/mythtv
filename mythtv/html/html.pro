include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple
win32-msvc*:QMAKE_COPY_DIR = 

html.path = $${PREFIX}/share/mythtv/html/
html.files = index.html overview.html menu.qsp
html.files += css images js misc setup samples tv
html.files += 3rdParty xslt video

INSTALLS += html

# Input
SOURCES += dummy.c

OTHER_FILES += \
    video/gallery.qsp
