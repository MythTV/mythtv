include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple

locales.path = $${PREFIX}/share/mythtv/locales/
locales.files = *.xml

INSTALLS += locales

# Input
SOURCES += dummy.c
